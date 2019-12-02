#pragma once

#include <queue>
#include <vector>
#include <PendingTraffic.h>
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

  void updateRecvNormal(uint32_t payloadRecv, uint32_t ackReceived)
  {
    expectedRecvSeq_ += payloadRecv;
  }

  void updateOnRecv(uint32_t payloadRecv, uint32_t ackReceived)
  {
    maxAckRecv_ = std::max(maxAckRecv_, ackReceived);

    LOG("RECEIVED, NEEDED ACK for  " << expectedRecvSeq_ + payloadRecv << " ack to send " << currentOutAckSeq() );
  }

  void updateRecvForSyn(ETH_HDR& hdr)
  {
    expectedRecvSeq_ += 1;
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
      LOG("Immediate reject");
      bytesRejectedAcked_ += payload.length();
      dontChange = true;
    }
    else
    {
      LOG("Delayed reject");
    }

    uint32_t cutOffSeq = currentOutSeq() + seqOffset;
    uint32_t origSeq = currentOutSeq() + bytesProcessed;

    LOG("REJ data cut off seq"  << cutOffSeq );

    pendingInjections().addRejection(cutOffSeq, payload, rejPayloadSeq, rejPayload, dontChange, origSeq);
  }

  void setTargetEndpoint(const EndPoint& ep)
  {
    target_ = ep;
  }

  bool processingDup(ETH_HDR& hdr)
  {
    if (expectedRecvSeq_ >  ntohl(hdr.seqNum))
    {
          LOG("DUP " <<  ntohl(hdr.seqNum) << " ,  "<< expectedRecvSeq_);
          return true;
    }
    return false;
  }

private:

  static void fillInfo(ETH_HDR& ethernetPkt,  const TcpStreamInfo& streamInfo)
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

       //pkt.tcp().window(12000);
       //pkt.tcp().winscale(1);

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
  }

  void sendPassPayloadNoCopy(std::string_view& payload)
  {
    preparePassPayloadNoCopy(payload);
    /// !!! Need to use the original QP, bz of the protection domain
    inSocket().sendNoCopy(payload);
    inSocket().pollSend();
  }

  void sendPassPayloadNoCopyOrigSeqNums(const std::string_view& data)
  {
    ETH_HDR& ethernetPkt  = const_cast<ETH_HDR&> (*reinterpret_cast<const ETH_HDR*> (data.data()));
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
  }
public:

  void processDup(std::string_view data)
  {
    std::vector<InjectionDetails> rejectedIntervals;

    ETH_HDR* hdr = (ETH_HDR*) data.data();

    uint32_t recvSeq = ntohl(hdr->seqNum);
    uint32_t outSeq = pendingInjections().calculateSeqNumForDup(recvSeq, bytesAdded_, bytesRejected_, rejectedIntervals);

    std::string_view dupData( hdr->data(), hdr->tcpLen());

    uint32_t dataProcessed = 0;
    uint32_t passDataAdded = 0;

    for (const auto& rejectedInterval : rejectedIntervals)
    {
      uint32_t intervalOrigSeqNum = rejectedInterval.myOrigSequence();

      if (intervalOrigSeqNum > outSeq + dataProcessed)
      {
          uint32_t toPassIntervalLen = intervalOrigSeqNum - outSeq - dataProcessed;
          uint32_t toPassDataOutSequence = outSeq + passDataAdded;

          std::string_view toReplay = dupData.substr(dataProcessed, toPassIntervalLen);
          sendExactPayload(toReplay, toPassDataOutSequence);
          dataProcessed += toReplay.size();
          passDataAdded += toReplay.size();
          LOG("Passthrough data:" << toReplay);
      }

      LOG("Rejected data:" << rejectedInterval.payload());
      reversePathContext().sendExactPayload(rejectedInterval.rejPayload(), rejectedInterval.rejSeq());
      dataProcessed += rejectedInterval.payload().size();
    }

    LOG ("Last passthrough data:" << dataProcessed  << " , len " <<  dupData.length());
    if (dataProcessed < dupData.length() )
    {
      std::string_view toReplay = dupData.substr(dataProcessed);
      LOG("Final pass through " << toReplay );

      sendExactPayload(toReplay, outSeq + passDataAdded);
    }
  }

protected:
  void prepareAddedPayload (const std::string_view& payload, std::string_view& outData)
  {
      prepareSequencedPayload(payload,  currentOutSeq(), outData);

      if (payload.length())
      {
        pendingInjections().addInjection(currentOutSeq(), payload);
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
      ETH_HDR& ethernetPkt  = const_cast<ETH_HDR&> (*reinterpret_cast<const ETH_HDR*> (data.data()));
      fillInfo(ethernetPkt, outTcp_);
      ethernetPkt.seqNum = htonl(currentOutSeq());
      ethernetPkt.ackNum = htonl(currentOutAckSeq());
  }

protected:
  uint32_t bytesRejected_;
  uint32_t bytesRejectedAcked_;

protected:
  uint32_t bytesAdded_;
  uint32_t bytesAddedAcked_;

protected:
  uint32_t expectedRecvSeq_;

public:
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

  bool processingSYN(ETH_HDR& hdr, uint32_t bytes)
  {
    if(hdr.f.syn())
    {
      LOG("SYN Received");

      clear();

      std::string_view pkt (reinterpret_cast<char*> (&hdr), bytes);
      TCPPacket dumper(pkt);


      inTcp_.init(dumper.ether().src_addr(), dumper.ether().dst_addr(),
                  dumper.ip().src_addr(), dumper.ip().dst_addr(),
                  dumper.tcp().sport(), dumper.tcp().dport()
                );

      outTcp_.init(dumper.ether().dst_addr(), target_.outMac_,
                   dumper.ip().dst_addr(), target_.outIP_,
                   dumper.tcp().sport(), target_.outPort_
                 );

      reversePathContext().outTcp_.init(dumper.ether().dst_addr(), dumper.ether().src_addr(),
                              dumper.ip().dst_addr(), dumper.ip().src_addr(),
                              dumper.tcp().dport(), dumper.tcp().sport()
                            );


      reversePathContext().inTcp_.init(target_.outMac_, dumper.ether().dst_addr(),
                           target_.outIP_, dumper.ip().dst_addr(),
                           target_.outPort_, dumper.tcp().sport()
                         );

      expectedRecvSeq_ = ntohl(hdr.seqNum);

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

  bool processingSYN(ETH_HDR& hdr, uint32_t bytes)
  {
    if(hdr.f.syn() && hdr.f.ack())
    {
      LOG("SYN ACK Received. Session is established");
      clear();
      expectedRecvSeq_ = ntohl(hdr.seqNum);
      return true;
    }
    return false;
  }
};
