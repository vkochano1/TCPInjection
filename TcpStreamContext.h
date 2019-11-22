#pragma once

#include <TcpStreamInfo.h>
#include <queue>

class Context
{
public:
  using AckInfo = std::pair<uint32_t, uint32_t>;
  using AckQueue = std::queue<AckInfo>;

public:
  Context()
  {
    clear();
  }

  void clear()
  {
    expectedRecvSeq_ = 0;
    bytesRejected_ = 0;
    bytesAdded_ = 0;
    bytesAddedAcked_ = 0;
    bytesRejectedAcked_ = 0;
    maxAckSent_ = 0;
    while(!ackQueue_.empty()) ackQueue_.pop();
  }

  void processAckQueue(size_t ackNum)
  {
    while (!ackQueue_.empty())
    {
      auto& p = ackQueue_.front();
      if (ackNum < p.first)
      {
        break;
      }
      bytesAddedAcked_ += p.second;
      ackQueue_.pop();
    }
  }

  void addAckInfo(size_t seq, size_t len)
  {
    ackQueue_.push( AckInfo(seq + len, len));
  }

  uint32_t outSeq() const
  {
    return expectedRecvSeq_ + bytesAdded_ - bytesRejected_;
  }

  uint32_t ackSeq(ETH_HDR& hdr, Context& counter)
  {
    return  ntohl(hdr.ackNum) - counter.bytesAddedAcked_ + counter.bytesRejectedAcked_;
  }

  void updateRecvNormal(ETH_HDR& hdr)
  {
    expectedRecvSeq_ += hdr.tcpLen();
  }

  void updateRecvForSyn(ETH_HDR& hdr)
  {
    expectedRecvSeq_ += 1;
  }

  void updateReject(const ETH_HDR& h)
  {
    bytesRejected_ += h.tcpLen();
    bytesRejectedAcked_ += h.tcpLen();
  }

  void updateAdded(std::string_view addon)
  {
    addAckInfo(outSeq(), addon.length());
    bytesAdded_ += addon.length();
  }

  void prepare(char* recvBuf, size_t bytes, Context& c, const TcpStreamInfo& streamInfo )
  {
      ETH_HDR* h  = (struct ETH_HDR*) recvBuf;

      streamInfo.fillSourceIP(h->src_ip);
      streamInfo.fillDestIP(h->dst_ip);

      streamInfo.fillSourceMAC(h->src_mac);
      streamInfo.fillDestMAC(h->dst_mac);

      streamInfo.fillSourcePort(h->sp);
      streamInfo.fillDestPort(h->dp);

      h->seqNum = htonl(outSeq());
      maxAckSent_ =   std::max(maxAckSent_, ackSeq(*h, c));
      h->ackNum = htonl(maxAckSent_);
  }

  static void prepareAddedPayload(const std::string_view& payload
                  , const TcpStreamInfo& streamInfo
                  , uint32_t seqNum
                  , uint32_t ackNum
                  , char*  outBuf
                  , size_t& bytes)
  {
       TCPPacket pkt;
       pkt.setSrcIP(streamInfo.sourceIP().to_string());
       pkt.setDstIP(streamInfo.destIP().to_string());

       pkt.setSrcPort(streamInfo.sourcePort());
       pkt.setDstPort(streamInfo.destPort());

       pkt.setSrcMAC(streamInfo.sourceMAC().to_string());
       pkt.setDstMAC(streamInfo.destMAC().to_string());

       //pkt.tcp().window(70);
       //pkt.tcp().winscale(1);

       pkt.setSeq(seqNum);
       pkt.setAckSeq(ackNum);

       pkt.setData(payload.data(), payload.length());

       auto serialized = pkt.ether().serialize();

       std::memcpy(outBuf,&serialized[0], serialized.size());
       bytes = serialized.size();
  }

  void setTargetEndpoint(const EndPoint& ep)
  {
    target_ = ep;
  }

  bool processingDup(ETH_HDR& hdr, size_t& diff)
  {
    if (expectedRecvSeq_ >  ntohl(hdr.seqNum))
    {
          LOG("DUP " <<  ntohl(hdr.seqNum) << " , "<< expectedRecvSeq_);
          diff = expectedRecvSeq_ -  ntohl(hdr.seqNum);
          return true;
    }
    return false;
  }

public:
  uint32_t expectedRecvSeq_;
  uint32_t bytesRejected_;
  uint32_t bytesRejectedAcked_;

  uint32_t bytesAdded_;
  int32_t bytesAddedAcked_;
  uint32_t maxAckSent_;

public:
  TcpStreamInfo inTcp_;
  TcpStreamInfo outTcp_;
  EndPoint      target_;

  AckQueue ackQueue_;
  AckQueue rejQueue_;
};


class InContext : public Context
{
public:
  using PendingConnections = std::unordered_map<uint32_t, uint32_t>;
public:

  bool processingSYN(ETH_HDR& hdr, size_t bytes, Context& counter)
  {
    if(hdr.f.syn())
    {
      LOG("SYN RECEIVED");

      clear();

      TCPDumper dumper(reinterpret_cast<char*> (&hdr), bytes);


      inTcp_.init(dumper.ether().src_addr(), dumper.ether().dst_addr(),
                  dumper.ip().src_addr(), dumper.ip().dst_addr(),
                  dumper.tcp().sport(), dumper.tcp().dport()
                );

      outTcp_.init(dumper.ether().dst_addr(), target_.outMac_,//dumper.ether().src_addr(),
                   dumper.ip().dst_addr(), target_.outIP_, // dumper.ip().src_addr(),
                   dumper.tcp().sport(), target_.outPort_
                 );



      /*outTcp_.init(dumper.ether().dst_addr(), dumper.ether().src_addr(),
                              dumper.ip().dst_addr(), dumper.ip().src_addr(),
                              dumper.tcp().sport(), dumper.tcp().dport()
                            );*/


      counter.outTcp_.init(dumper.ether().dst_addr(), dumper.ether().src_addr(),
                              dumper.ip().dst_addr(), dumper.ip().src_addr(),
                              dumper.tcp().dport(), dumper.tcp().sport()
                            );

     /*counter.inTcp_.init(dumper.ether().src_addr(), dumper.ether().dst_addr(),
                 dumper.ip().src_addr(), dumper.ip().dst_addr(),
                 dumper.tcp().dport(), dumper.tcp().sport()
               );*/

      counter.inTcp_.init(target_.outMac_, dumper.ether().dst_addr(),
                           target_.outIP_, dumper.ip().dst_addr(),
                           target_.outPort_, dumper.tcp().sport()
                         );

      expectedRecvSeq_ = ntohl(hdr.seqNum);

      return true;
    }

    return false;
  }


};


class OutContext : public Context
{
public:
  bool processingSYN(ETH_HDR& hdr, size_t bytes, Context& counter)
  {
    if(hdr.f.syn() && hdr.f.ack())
    {
      LOG("SYN ACK Received. session is established");
      clear();
      expectedRecvSeq_ = ntohl(hdr.seqNum);
      return true;
    }
    return false;
  }
};
