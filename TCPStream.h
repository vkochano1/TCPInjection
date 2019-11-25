#pragma once

#include <TcpStreamContext.h>

template<typename InSocketType, typename OutSocketType, typename PayloadProcessor>
class Stream
{
public:
  Stream(InSocketType& in, OutSocketType& out, PayloadProcessor& processor)
     : in_(in)
     , out_(out)
     , processor_ (processor)
  {
  }

  void reject(std::string_view data, std::string_view addon)
  {
    /// 1. empty frame with transformed ack
    ETH_HDR* h  = (ETH_HDR*) data.data();
    in_.context().prepare(data, out_.context(),in_.context().outTcp_);

    TCPDumper dumper(data.data(), data.size());
    TCPPacket pkt(dumper);

    pkt.setData("", 0);
    auto serialized = pkt.ether().serialize();

    //std::memcpy(in_.sendBuf_,&serialized[0], serialized.size());
    std::string_view out (&serialized[0], serialized.size());

    in_.send(out);
    in_.pollSend();



   /// 2. for rejected packet
    in_.context().updateReject(*h);


    size_t written = 0;
    out_.context().prepareAddedPayload(addon
                              , out_.context().outTcp_, out_.context().outSeq()
                              , out_.context().maxAckSent_, out_.sendBuf_
                              , written);

    std::string_view outNC(out_.sendBuf_, written);
    out_.sendNoCopy(outNC);
    out_.pollSend();

    out_.context().updateAdded(addon);
  }

  void poll ()
  {
    size_t id;
    std::string_view data;

    if(in_.pollRecv(id, data))
    {
     char* buf = data.data();
     size_t bytes = data.length();
     ETH_HDR* h  = (struct ETH_HDR*) buf;
     size_t diff = 0;

      if ( in_.context().processingSYN(*h, bytes, out_.context()) )
      {
        in_.context().prepare(data, out_.context(),in_.context().outTcp_);
        in_.sendNoCopy(data);
        in_.pollSend();
        in_.context().updateRecvForSyn(*h);
      }
      else if (in_.context().processingDup(*h, diff))
      {
        in_.context().prepareDup(data, out_.context(),in_.context().outTcp_, diff);
        in_.sendNoCopy(data);
        in_.pollSend();
      }
      else // normal flow
      {
         out_.context().processReceivedAck(ntohl(h->ackNum));

         if (h->tcpLen() > 0 && !processor_.validate(*h))
         {
           std::string_view rejPayload = processor_.reject();
           reject(data, rejPayload);
         }
         else
         {
           in_.context().prepare(data, out_.context(),in_.context().outTcp_);
           in_.sendNoCopy(data);
           in_.pollSend();
         }

         in_.context().updateRecvNormal(*h);
      }

      in_.postRecv();
    }
 }
public:
 InSocketType&  in_;
 OutSocketType& out_;
 PayloadProcessor& processor_;
};
