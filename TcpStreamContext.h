#pragma once

#include <vector>
#include <InjectionsAndRejections.h>
#include <QPSocket.h>
#include <TcpStreamInfo.h>

template<typename SocketType>
class Context
{
public:
  Context(Context& reversePathContext, SocketType& inSocket, SocketType& outSocket)
    : inSocket_  (inSocket)
    , outSocket_  (outSocket)
    , reversePathContext_(reversePathContext)
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
    maxAckRecv_= 0;
    pendingInjections_.clear();
  }

  SocketType& inSocket()
  {
    return inSocket_;
  }

  SocketType& outSocket()
  {
    return outSocket_;
  }

  Context& reversePathContext()
  {
    return reversePathContext_;
  }

  const Context& reversePathContext() const
  {
    return reversePathContext_;
  }

  PendingInjections&  pendingInjections ()
  {
    return pendingInjections_;
  }

public:
  void processReceivedAck(uint32_t ackNum)
  {
    pendingInjections().processReceivedAck(ackNum, bytesAddedAcked_, bytesRejectedAcked_);
  }

  uint32_t currentOutSeq() const
  {
    return expectedRecvSeq_ + bytesAdded_ - bytesRejected_;
  }

  uint32_t currentOutAckSeq() const
  {
    return  maxAckRecv_ - reversePathContext().bytesAddedAcked_ + reversePathContext().bytesRejectedAcked_;
  }

  void updateRecvNormal(TCPPacketLite& pkt)
  {
    expectedRecvSeq_ += pkt.tcpLen();
  }

  void updateAckOnRecv(uint32_t ackReceived)
  {
    maxAckRecv_ = std::max(maxAckRecv_, ackReceived);
  }

  void updateRecvSeqForSyn()
  {
    expectedRecvSeq_ += 1;
  }

  bool isDup(uint32_t seqNum, const std::string_view& payload) const
  {
    if (expectedRecvSeq_ > seqNum)
    {
      LOG("Processing duplicate. Expected seqNum is "
          <<  expectedRecvSeq_
          << " and received "
          << seqNum
         );

      if (payload.size() + seqNum > expectedRecvSeq_ )
      {
        std::exit(0);
      }
      return true;
    }

    return false;
  }

  bool isHigher(uint32_t seqNum)  const
  {
    return expectedRecvSeq_ <  seqNum;
  }


  bool isNewFlow(uint32_t seqNum)  const
  {
    return expectedRecvSeq_ == seqNum;
  }

  void commitReject(uint32_t lenRecv)
  {
    bytesRejected_ += lenRecv;
  }

  void updateReject(std::string_view payload, std::string_view rejPayload, uint32_t seqOffset, uint32_t bytesProcessed)
  {
    bool dontChange = false;
    uint32_t rejPayloadSeq = reversePathContext().currentOutSeq();
    if (reversePathContext().maxAckRecv_ == currentOutSeq() + seqOffset)
    {
      bytesRejectedAcked_ += payload.length();
      dontChange = true;
    }

    uint32_t cutOffSeq = currentOutSeq() + seqOffset;
    uint32_t origSeq = expectedRecvSeq_ + bytesProcessed;

    LOG("Reject data cut off sequence " << cutOffSeq);

    pendingInjections().addRejection(cutOffSeq, payload, rejPayloadSeq, rejPayload, dontChange, origSeq);
  }

  void setTargetEndpoint(const EndPoint& endpoint)
  {
    target_ = endpoint;
  }

private:
  static void fillInfo(TCPPacketLite& ethernetPkt,  const TcpStreamInfo& streamInfo)
  {
    streamInfo.fillSourceIP(&ethernetPkt.src_ip[0]);
    streamInfo.fillDestIP(&ethernetPkt.dst_ip[0]);

    streamInfo.fillSourceMAC(&ethernetPkt.src_mac[0]);
    streamInfo.fillDestMAC(&ethernetPkt.dst_mac[0]);

    streamInfo.fillSourcePort(ethernetPkt.sp);
    streamInfo.fillDestPort(ethernetPkt.dp);
  }

  static void prepareOutPayloadImpl (const std::string_view& payload
                  , const TcpStreamInfo& streamInfo
                  , uint32_t seqNum
                  , uint32_t ackNum
                  , std::string_view& outData)
  {
       TCPPacket pkt;
       pkt.setSrcIP(streamInfo.sourceIP().to_string());
       pkt.setDstIP(streamInfo.destIP().to_string());

       pkt.setSrcPort(streamInfo.sourcePort());
       pkt.setDstPort(streamInfo.destPort());

       pkt.setSrcMAC(streamInfo.sourceMAC().to_string());
       pkt.setDstMAC(streamInfo.destMAC().to_string());

       pkt.tcp().window(444);
       pkt.setSeq(seqNum);
       pkt.setAckSeq(ackNum);

       pkt.setData(payload.data(), payload.length());

       auto serialized = pkt.ether().serialize();

       std::memcpy(const_cast<char*>(outData.data()), &serialized[0], serialized.size());
       outData.remove_suffix(outData.size() - serialized.size());
  }

public:
  void sendPayload(const std::string_view& payload, bool noSend = false)
  {
    std::string_view out = outSocket().reserveSendBuf();

    prepareAddedPayload(payload, out);

    if (!noSend )
    {
      outSocket().sendNoCopy(out);
      outSocket().pollSend();
    }

    LOG("Sent reject payload " << noSend << " " << TCPPacketLite::fromData(out));
  }

  void sendAck()
  {
    std::string_view out = outSocket().reserveSendBuf();

    std::string_view payload;
    prepareAddedPayload(payload, out);
    outSocket().sendNoCopy(out);
    outSocket().pollSend();
    LOG("Sent ack " << TCPPacketLite::fromData(out));
  }


  void sendPostponedPayload(const std::string_view& payload, bool noSend = false)
  {
    std::string_view out = outSocket().reserveSendBuf();

    preparePostponedPayload(payload, out);

    if (!noSend )
    {
      outSocket().sendNoCopy(out);
      outSocket().pollSend();
    }

    LOG("Sent postponed payload " << noSend << " " << TCPPacketLite::fromData(out));
  }

  void sendPassPayload(const std::string_view& payload, uint32_t& seqOffset, bool noSend = false)
  {
    std::string_view out = outSocket().reserveSendBuf();
    preparePassPayload(payload, seqOffset, out);

    if (!noSend)
    {
      outSocket().sendNoCopy(out);
      outSocket().pollSend();
    }

    LOG("Sent pass payload : " << " " << TCPPacketLite::fromData(out));
  }

  void sendPassPayloadNoCopy(std::string_view& payload)
  {
    preparePassPayloadNoCopy(payload);
    /// !!! Need to use the original QP, bz of the protection domain
    inSocket().sendNoCopy(payload);
    inSocket().pollSend();

    LOG("Sent payload "<< " " << TCPPacketLite::fromData(payload));
  }

  void sendPassPayloadNoCopyOrigSeqNums(std::string_view& data)
  {
    TCPPacketLite& ethernetPkt  = TCPPacketLite::fromData(data);
    fillInfo(ethernetPkt, outTcp_);

    inSocket().sendNoCopy(data);
    inSocket().pollSend();
  }

  void sendExactPayload(const std::string_view& payload, uint32_t seq)
  {
    std::string_view out = outSocket().reserveSendBuf();
    prepareSequencedPayload(payload,seq, out);
    outSocket().sendNoCopy(out);
    outSocket().pollSend();

    LOG("Sent dup payload : "<< " " << TCPPacketLite::fromData(out));
  }

public:

  void processDup(std::string_view data)
  {
    TCPPacketLite& hdr = TCPPacketLite::fromData(data);
    uint32_t recvSeq = hdr.seqNum();
    std::string_view dupData = hdr.payload();


    bool noRsend = pendingInjections().processDupPayload(recvSeq, dupData, bytesAdded_, bytesRejected_,
      [this] (uint32_t seqNum, std::string_view payload)
      {
        sendExactPayload( payload, seqNum);
      }
      ,
      [this] (uint32_t seqNum, std::string_view payload)
      {
        reversePathContext().sendExactPayload(payload, seqNum);
      }
      ,
      reversePathContext().currentOutAckSeq()
    );

    if (noRsend)
    {
      reversePathContext().sendAck();
    }
  }

protected:
  void prepareAddedPayload (const std::string_view& payload, std::string_view& outData)
  {
      prepareSequencedPayload(payload,  currentOutSeq(), outData);

      //if (payload.length())
      {
        pendingInjections().addInjection(currentOutSeq(), payload, expectedRecvSeq_);
        bytesAdded_ += payload.length();
      }
  }

  void preparePostponedPayload(const std::string_view& payload, std::string_view& outData)
  {
      prepareSequencedPayload(payload,  currentOutSeq(), outData);

      //if (payload.length())
      {
        pendingInjections().addInjection(currentOutSeq(), payload, expectedRecvSeq_ - payload.size());
        bytesAdded_ += payload.length();
      }
  }


  void preparePassPayload (const std::string_view& payload, uint32_t& seqOffset, std::string_view& outData)
  {
      prepareSequencedPayload(payload, currentOutSeq() + seqOffset,  outData );

      seqOffset += payload.length();
  }

  void prepareSequencedPayload (const std::string_view& payload,  uint32_t outSeqeunce, std::string_view& outData)
  {
      prepareOutPayloadImpl(payload
                           , outTcp_
                           , outSeqeunce
                           , currentOutAckSeq()
                           , outData);
  }

  void preparePassPayloadNoCopy(std::string_view& data)
  {
      TCPPacketLite& ethernetPkt  = TCPPacketLite::fromData(data);
      fillInfo(ethernetPkt, outTcp_);
      ethernetPkt.seqNum(currentOutSeq());
      ethernetPkt.ackNum(currentOutAckSeq());
  }

protected:
  uint32_t bytesRejected_;
  uint32_t bytesRejectedAcked_;

protected:
  uint32_t bytesAdded_;
  uint32_t bytesAddedAcked_;

protected:
  uint32_t expectedRecvSeq_;

protected:
  mutable uint32_t maxAckRecv_;

public:
  TcpStreamInfo inTcp_;
  TcpStreamInfo outTcp_;
  EndPoint      target_;
  PendingInjections  pendingInjections_;
  SocketType& inSocket_;
  SocketType& outSocket_;
  Context& reversePathContext_;
};


class InContext : public Context<QPSocket>
{
public:
  using PendingConnections = std::unordered_map<uint32_t, uint32_t>;
  using Base = Context<QPSocket>;
public:

  InContext (Base& reversePathContext, QPSocket& inSocket, QPSocket& outSocket)
  : Base(reversePathContext, inSocket, outSocket)
  {

  }

  bool processingSYN(TCPPacketLite& hdr, std::string_view& data)
  {
    if(hdr.f.syn())
    {
      LOG("SYN Received. Waiting for SYN ACK...");

      clear();

      TCPPacket pkt(data);


      inTcp_.init(pkt.ether().src_addr(), pkt.ether().dst_addr(),
                  pkt.ip().src_addr(), pkt.ip().dst_addr(),
                  pkt.tcp().sport(), pkt.tcp().dport()
                );

      outTcp_.init(pkt.ether().dst_addr(), target_.outMac_,
                   pkt.ip().dst_addr(), target_.outIP_,
                   pkt.tcp().sport(), target_.outPort_
                 );

      reversePathContext().outTcp_.init(pkt.ether().dst_addr(), pkt.ether().src_addr(),
                              pkt.ip().dst_addr(), pkt.ip().src_addr(),
                              pkt.tcp().dport(), pkt.tcp().sport()
                            );


      reversePathContext().inTcp_.init(target_.outMac_, pkt.ether().dst_addr(),
                           target_.outIP_, pkt.ip().dst_addr(),
                           target_.outPort_, pkt.tcp().sport()
                         );

      expectedRecvSeq_ = hdr.seqNum();

      updateRecvSeqForSyn();

      return true;
    }

    return false;
  }

};


class OutContext : public Context<QPSocket>
{
public:

  using Base = Context<QPSocket>;

  OutContext (Base& reversePathContext, QPSocket& inSocket, QPSocket& outSocket)
  : Base (reversePathContext, inSocket, outSocket)
  {
  }

  bool processingSYN(TCPPacketLite& hdr, std::string_view& data)
  {
    if(hdr.f.syn() && hdr.f.ack())
    {
      LOG("SYN ACK Received. TCP session is established.");
      clear();
      expectedRecvSeq_ = hdr.seqNum();
      updateRecvSeqForSyn();
      return true;
    }

    return false;
  }
};
