#pragma once

#include <TcpStreamContext.h>

template<typename ContextType, typename PayloadProcessor>
class Stream
{
public:
  Stream(ContextType& context, PayloadProcessor& processor)
     : context_(context)
     , processor_(processor)
  {
    nPoints_ = 0;
    avgRecvTime_ = 0;
  }

  ContextType& context()
  {
    return context_;
  }

  void reject_(std::string_view payload, std::string_view rejPayload, uint32_t& seqOffset, uint32_t& bytesProcessed,  bool noSend)
  {
    context().updateReject(payload, rejPayload, seqOffset, bytesProcessed);
    bytesProcessed += payload.size();
    context().reversePathContext().sendPayload(rejPayload, noSend);
  }

  void pass_(std::string_view payload, uint32_t& seqOffset, uint32_t& bytesProcessed,  bool noSend)
  {
    context().sendPassPayload(payload, seqOffset, noSend);
    bytesProcessed += payload.size();
  }

  void processNewData(std::string_view& pkt, std::string_view& payload)
  {
    bool valid = true;
    uint32_t bytesRejected = 0;
    uint32_t seqOffset = 0;
    uint32_t bytesProcessed = 0;
    uint32_t lenProcessed = 0;

    do
    {
      valid = processor_.validate(payload, lenProcessed);

      if (valid)
      {
        if (lenProcessed == payload.size() && !bytesRejected)
        {
          context().sendPassPayloadNoCopy(pkt);
          endRecvTime_ =  Utils::rdtsc();
          avgRecvTime_ = (avgRecvTime_ * nPoints_ + (endRecvTime_ - startRecvTime_)) / (nPoints_ + 1);

          if (++nPoints_ % 50000)
          {
            LOG("AvgNumber of ticks " << avgRecvTime_ * 0.25641);
            LOG("Last of ticks " << endRecvTime_ - startRecvTime_);
          }
        }
        else
        {
          std::string_view toPass = payload.substr(0, lenProcessed);
          pass_(toPass, seqOffset, bytesProcessed, false);
        }
      }
      else
      {
        bytesRejected += lenProcessed;
        std::string_view rejPayload = processor_.reject();
        std::string_view toRej = payload.substr(0, lenProcessed);
        reject_(toRej, rejPayload, seqOffset,  bytesProcessed, false);
      }

      payload.remove_prefix(lenProcessed);
    } while(payload.size());

    if (bytesRejected)
    {
       context().commitReject(bytesRejected);
    }

  }

  void poll ()
  {
    size_t id;
    std::string_view data;
    size_t avg = 0;
    size_t counter = 0;

    if(context().inSocket().pollRecv(id, data))
    {
      startRecvTime_ =  Utils::rdtsc();

     const char* buf = data.data();
     uint32_t bytes = data.length();
     ETH_HDR* h  = (struct ETH_HDR*) buf;

     uint32_t ackRecv = ntohl(h->ackNum);
     uint32_t lenRecv = h->tcpLen();
     std::string_view payload = std::string_view(h->data(), lenRecv );

     //LOG("Received seq " << ntohl(h->seqNum) <<", ack " << ackRecv << ", len " << lenRecv << "======================================================");
     context().updateOnRecv(lenRecv, ackRecv);

      if (context().processingSYN(*h, bytes) )
      {
        context().sendPassPayloadNoCopyOrigSeqNums(data);
        context().updateRecvForSyn(*h);
      }
      else if (context().processingDup(*h))
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

         context().updateRecvNormal(lenRecv, ackRecv);
      }

      context().inSocket().postRecv();
    }
 }
public:
 ContextType& context_;
 PayloadProcessor& processor_;

 size_t startRecvTime_;
 size_t endRecvTime_;
 size_t  avgRecvTime_;
 size_t nPoints_;

};
