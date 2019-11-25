#pragma once

#include <TcpStreamInfo.h>
#include <queue>


struct InjectionDetails
{
    enum InjectionType
    {
      Added,
      Reject
    };

    InjectionDetails()
    {
        mySequence_= 0;
    }

    InjectionDetails( InjectionType injectionType
                    , uint32_t seq
                   , std::string_view payload)
    {
      mySequence_ = seq;
      payload_ = payload;
      injectionType_ = injectionType;
    }

    InjectionType injectionType () const
    {
        return injectionType_;
    }

    uint32_t expectedAck() const
    {
      return mySequence_ + payloadSize();
    }

    uint32_t mySequence() const
    {
      return mySequence_;
    }

    uint32_t payloadSize() const
    {
      return payload_.size();
    }

    InjectionType injectionType_;
    uint32_t mySequence_;
    std::string_view payload_;
};


struct PendingInjections : protected std::deque<InjectionDetails>
{

  void processReceivedAck(size_t ackNum, uint32_t& bytesAcked, uint32_t& bytesRejectedAcked_)
  {
    while (!this->empty())
    {
      const auto& element = this->front();
      if (ackNum < element.expectedAck())
      {
        break;
      }
      if (element.injectionType() == InjectionDetails::Added)
        bytesAcked  += element.payloadSize();
      else
      {
        bytesRejectedAcked_ += element.payloadSize();
      }



      this->pop_front();
    }
  }

  uint32_t calculateSeqNumForDup(uint32_t inSeq, uint32_t bytesAlreadyAdded, uint32_t bytesAlreadyRejected)
  {
      uint32_t outSeq = inSeq + bytesAlreadyAdded - bytesAlreadyRejected;

      for (auto it = this->rbegin(); it != rend(); ++it )
      {
          const auto& element = *it;

          if (element.mySequence() > inSeq)
          {
            if (element.injectionType() == InjectionDetails::Added)
              outSeq -= element.payloadSize();
            else
              outSeq += element.payloadSize();
          }
          else
          {
            break;
          }
      }
      return outSeq;
  }

  bool hasPendingInjections() const  { return !this->empty();}

  void clear()
  {
      while(!this->empty())
      {
         this->pop_front();
      }
  }

  void addInjection(size_t seq, std::string_view payload)
  {
    this->push_back(InjectionDetails(InjectionDetails::Added, seq, payload));
  }

  void addRejection(size_t seq, std::string_view payload)
  {
    this->push_back(InjectionDetails(InjectionDetails::Reject, seq, payload));
  }
};


class Context
{

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
    pendingInjections_.clear();
  }

  PendingInjections&  pendingInjections () { return pendingInjections_; }

  void processReceivedAck(uint32_t ackNum)
  {
    pendingInjections().processReceivedAck(ackNum, bytesAddedAcked_, bytesRejectedAcked_);
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


    //bytesRejectedAcked_ += h.tcpLen();

    std::string_view tmp (nullptr, h.tcpLen());
    pendingInjections().addRejection(outSeq(), tmp);
  }

  void updateAdded(std::string_view addon)
  {
    pendingInjections().addInjection(outSeq(), addon);
    bytesAdded_ += addon.length();
  }

  static void fillInfo(ETH_HDR& ethernetPkt,  const TcpStreamInfo& streamInfo)
  {
    streamInfo.fillSourceIP(ethernetPkt.src_ip);
    streamInfo.fillDestIP(ethernetPkt.dst_ip);

    streamInfo.fillSourceMAC(ethernetPkt.src_mac);
    streamInfo.fillDestMAC(ethernetPkt.dst_mac);

    streamInfo.fillSourcePort(ethernetPkt.sp);
    streamInfo.fillDestPort(ethernetPkt.dp);
  }

  void prepare(std::string_view& data, Context& c, const TcpStreamInfo& streamInfo )
  {
      ETH_HDR* h  = (struct ETH_HDR*) data.data();
      fillInfo(*h, streamInfo);

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

       //pkt.tcp().window(12000);
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

  void prepareDup(std::string_view& data, Context& c, const TcpStreamInfo& streamInfo, size_t diff )
  {
      /*if (pendingInjections().hasPendingInjections())
      {
        std::cerr << "Pending injections !!!!";
        std::exit(-1);
      }
      else
      {
        std::cerr << "Trying to proceed" << std::endl;
      }*/


      ETH_HDR* h  = (struct ETH_HDR*) data.data();
      uint32_t recvSeq = ntohl(h->seqNum);
      uint32_t passThrSeq = recvSeq + bytesAdded_ + bytesRejected_;
      uint32_t newSeq = pendingInjections().calculateSeqNumForDup(recvSeq, bytesAdded_, bytesRejected_);

      std::cerr << "Received seq " << recvSeq << " calculated seq " << newSeq <<" pass thr seq " << passThrSeq << " " << pendingInjections().hasPendingInjections() ;

      fillInfo(*h, streamInfo);

      h->seqNum = htonl(passThrSeq);
      maxAckSent_ =   std::max(maxAckSent_, ackSeq(*h, c));
      h->ackNum = htonl(maxAckSent_);
  }

public:
  uint32_t expectedRecvSeq_;
  uint32_t bytesRejected_;
  uint32_t bytesRejectedAcked_;

  uint32_t bytesAdded_;
  uint32_t bytesAddedAcked_;
  uint32_t maxAckSent_;

public:
  TcpStreamInfo inTcp_;
  TcpStreamInfo outTcp_;
  EndPoint      target_;

  PendingInjections  pendingInjections_;
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


      counter.outTcp_.init(dumper.ether().dst_addr(), dumper.ether().src_addr(),
                              dumper.ip().dst_addr(), dumper.ip().src_addr(),
                              dumper.tcp().dport(), dumper.tcp().sport()
                            );


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
