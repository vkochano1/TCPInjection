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

  void reject(ETH_HDR* h, std::string_view addon)
  {
    in_.context().updateReject(*h);

    size_t written = 0;
    out_.context().prepareAddedPayload(addon
                              , out_.context().outTcp_, out_.context().outSeq()
                              , out_.context().maxAckSent_, out_.sendBuf_
                              , written);

    out_.send(written, out_.sendBuf_);
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
        in_.context().prepare(buf, bytes, out_.context(),in_.context().outTcp_);
        in_.send(bytes, buf);
        in_.pollSend();
        in_.context().updateRecvForSyn(*h);
      }
      else if (in_.context().processingDup(*h, diff))
      {

      }
      else // normal flow
      {
         out_.context().processAckQueue(ntohl(h->ackNum));

         if (h->tcpLen() > 0 && !processor_.validate(*h))
         {
           std::string_view rejPayload = processor_.reject();
           reject(h, rejPayload);
         }
         else
         {
           in_.context().prepare(buf, bytes, out_.context(),in_.context().outTcp_);
           in_.send(bytes, buf);
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
