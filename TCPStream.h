#pragma once

#include <TcpStreamContext.h>
#include <ValidatorBase.h>

template<typename ContextType, typename PayloadProcessor>
class Stream
{
public:
  Stream(ContextType& context, PayloadProcessor& processor)
     : context_(context)
     , processor_(processor)
  {

  }

  ContextType& context()
  {
    return context_;
  }

  void poll ()
  {
    size_t id;
    std::string_view data;

    if(context().inSocket().pollRecv(id, data))
    {
     timer_.recordStart();

     TCPPacketLite& tcpHeaderLite  = TCPPacketLite::fromData(data);
     uint32_t ackRecv = tcpHeaderLite.ackNum();
     uint32_t recvSeq = tcpHeaderLite.seqNum();
     std::string_view payload = tcpHeaderLite.payload();

     LOG("===========================  Received ==========================\n" << tcpHeaderLite);

     context().updateAckOnRecv(ackRecv);

      if (context().processingSYN(tcpHeaderLite, data) )
      {
        context().sendPassPayloadNoCopyOrigSeqNums(data);
      }
      else if (context().isDup(recvSeq))
      {
        context().processDup(data);
      }
      else // normal flow
      {
         context().reversePathContext().processReceivedAck(ackRecv);

         if (payload.size() == 0)
         {
           context().sendPassPayloadNoCopy(data);
         }
         else
         {
           processNewData(data, payload);
         }

         context().updateRecvNormal(tcpHeaderLite);
      }

      context().inSocket().postRecv();
    }
 }

protected:

  void reject_(std::string_view payload
              , std::string_view rejPayload, uint32_t& seqOffset
              , uint32_t& bytesProcessed
              , bool noSend)
  {
    context().updateReject(payload, rejPayload, seqOffset, bytesProcessed);
    bytesProcessed += payload.size();
    context().reversePathContext().sendPayload(rejPayload, noSend);

    LOG("Sent reject payload : " << rejPayload);
  }

  void pass_(std::string_view payload
            , uint32_t& seqOffset
            , uint32_t& bytesProcessed
            ,  bool noSend)
  {
    context().sendPassPayload(payload, seqOffset, noSend);
    bytesProcessed += payload.size();
  }

  void add_(std::string_view payload, bool noSend)
  {
    context().sendPostponedPayload(payload, noSend);
  }

  void processNewData(std::string_view& pkt, std::string_view& payload)
  {
    uint32_t bytesRejected = 0;
    uint32_t seqOffset = 0;
    uint32_t bytesProcessed = 0;

    do
    {
      std::string_view addedPayload;
      uint32_t lenProcessed = 0;

      auto status = processor_.validate(payload, lenProcessed, addedPayload );

      using StatusType = decltype(status);

      if (status == StatusType::Valid)
      {
        if (   lenProcessed == payload.size()
            && !bytesRejected)
        {
          context().sendPassPayloadNoCopy(pkt);
          timer_.recordEnd();
        }
        else
        {
          std::string_view toPass = payload.substr(0, lenProcessed);
          pass_(toPass, seqOffset, bytesProcessed, false);
        }
      }
      else if (status == StatusType::Invalid)
      {
        bytesRejected += lenProcessed;
        std::string_view rejPayload = addedPayload;
        std::string_view toRej = payload.substr(0, lenProcessed);
        reject_(toRej, rejPayload, seqOffset,  bytesProcessed, false);
      }
      else if (status == StatusType::PartialPayload)
      {
        // emulating reject with empty reject message
        bytesRejected += lenProcessed;
        std::string_view rejPayload;
        std::string_view toRej = payload.substr(0, lenProcessed);

        reject_(toRej, rejPayload, seqOffset,  bytesProcessed, false);

        LOG("Partial message " << toRej << ". Don't process.");
      }
      else if (status == StatusType::PayloadAdded)
      {
        LOG("Payload added " << addedPayload);
        add_(addedPayload, false);

        if (lenProcessed)
        {
          std::string_view toPass = payload.substr(0, lenProcessed);
          pass_(toPass, seqOffset, bytesProcessed, false);
          LOG("Additional payload processed : " << toPass);
        }
      }

      payload.remove_prefix(lenProcessed);
    }
    while(payload.size());

    if (bytesRejected)
    {
       context().commitReject(bytesRejected);
    }
  }

protected:
 ContextType& context_;
 PayloadProcessor& processor_;
 Utils::DummyTimer timer_;


};
