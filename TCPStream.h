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
    std::string_view data;

    if(context().inSocket().pollRecv(data))
    {
     timer_.recordStart();

     TCPPacketLite& tcpHeaderLite  = TCPPacketLite::fromData(data);
     uint32_t ackRecv = tcpHeaderLite.ackNum();
     uint32_t recvSeq = tcpHeaderLite.seqNum();
     std::string_view payload = tcpHeaderLite.payload();

     LOG("===========================  Received ==========================");
     LOG(tcpHeaderLite);

     context().updateAckOnRecv(ackRecv);

      if (context().processingSYN(tcpHeaderLite, data) )
      {
        context().sendPassPayloadNoCopyOrigSeqNums(data);
      }
      else if (context().isNewFlow(recvSeq))
      {
        context().reversePathContext().processReceivedAck(ackRecv);

        if (payload.size())
        {
          processNewData(data, payload);
        }
        else
        {
          context().sendPassPayloadNoCopy(data);
        }

        context().updateRecvNormal(tcpHeaderLite);
      }
      else if (context().isDup(recvSeq, payload))
      {
        context().reversePathContext().processReceivedAck(ackRecv);

        context().processDup(data);
      }
      else if (context().isHigher(recvSeq))
      {
        LOG("Dropping packet with higher seqNum : " <<  recvSeq);
      }

      context().inSocket().postRecv();
    }
 }

protected:
  void reject_(std::string_view payload
              , std::string_view rejPayload, uint32_t& seqOffset
              , uint32_t& bytesProcessed)
  {
    context().updateReject(payload, rejPayload, seqOffset, bytesProcessed);
    bytesProcessed += payload.size();
    context().reversePathContext().sendPayload(rejPayload);

    LOG("Sent reject payload : " << rejPayload);
  }

  void pass_(std::string_view payload
            , uint32_t& seqOffset
            , uint32_t& bytesProcessed)
  {
    context().sendPassPayload(payload, seqOffset);
    bytesProcessed += payload.size();
  }

  void processNewData(std::string_view& pkt, std::string_view& payload)
  {
    uint32_t bytesRejected = 0;
    uint32_t seqOffset = 0;
    uint32_t bytesProcessed = 0;

    do
    {
      std::string_view outPayload;
      uint32_t lenProcessed = 0;

      auto status = processor_.validate(payload, lenProcessed, /*out*/outPayload);

      using StatusType = decltype(status);

      if (StatusType::Valid == status)
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
          pass_(toPass, seqOffset, bytesProcessed);
        }
      }
      else if (StatusType::Invalid == status)
      {
        bytesRejected += lenProcessed;
        std::string_view rejPayload = outPayload;
        std::string_view toRej = payload.substr(0, lenProcessed);
        reject_(toRej, rejPayload, seqOffset,  bytesProcessed);
      }
      else if (StatusType::PartialPayload == status)
      {
        // emulating reject with empty reject message
        bytesRejected += lenProcessed;
        std::string_view toRej = payload.substr(0, lenProcessed);
        reject_(toRej, std::string_view(), seqOffset,  bytesProcessed);

        LOG("Partial message " << toRej << " ->  don't process");
      }
      else if (StatusType::PayloadAdded == status)
      {
        LOG("Payload added :" << addedPayload);

        if (!lenProcessed)
          context().sendPayload(payload);
        else
          context().sendPostponedPayload(payload);

        if (lenProcessed)
        {
          std::string_view toPass = payload.substr(0, lenProcessed);
          pass_(toPass, seqOffset, bytesProcessed);
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
