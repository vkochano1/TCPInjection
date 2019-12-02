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
  }

  ContextType& context()
  {
    return context_;
  }

  void reject(std::string_view data, uint32_t lenRecv,  std::string_view addon)
  {
     /// 1. empty frame with transformed ack
     static  size_t c = 0;
     bool noSend = false;
     if (c++ % 2 == 1)
     {
        noSend = true;
        LOG("Don't send. Emulating re-transmits");
     }

    ETH_HDR* h = (ETH_HDR*) data.data();

    auto payload  = std::string_view(h->data(), h->tcpLen());
    auto pass1 = payload.substr(0,5);
    auto toRej1 = payload.substr(5, 10);
    auto pass2 = payload.substr(15, 5);
    auto toRej2 = payload.substr(20, 5);
    auto pass3 = payload.substr(25);

    uint32_t seqOffset = 0;
    context().sendPassPayload(pass1, seqOffset, noSend);
    context().updateReject(toRej1, addon, seqOffset, pass1.size());
    context().reversePathContext().sendPayload(addon, noSend);

    context().sendPassPayload(pass2, seqOffset, noSend);
    context().updateReject(toRej2, addon ,seqOffset, pass1.size() + toRej1.size() + pass2.size());
    context().reversePathContext().sendPayload(addon, noSend);

    context().sendPassPayload( pass3, seqOffset, noSend);

    context().commitReject(toRej1.size() + toRej2.size());
  }

  void poll ()
  {
    size_t id;
    std::string_view data;

    if(context().inSocket().pollRecv(id, data))
    {
     const char* buf = data.data();
     uint32_t bytes = data.length();
     ETH_HDR* h  = (struct ETH_HDR*) buf;

     uint32_t ackRecv = ntohl(h->ackNum);
     uint32_t lenRecv = h->tcpLen();

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

         if (lenRecv > 0 && !processor_.validate(*h))
         {
           std::string_view rejPayload = processor_.reject();
           reject(data, lenRecv, rejPayload);
         }
         else
         {
           context().sendPassPayloadNoCopy(data);
         }

         context().updateRecvNormal(lenRecv, ackRecv);
      }

      context().inSocket().postRecv();
    }
 }
public:
 ContextType& context_;
 PayloadProcessor& processor_;
};
