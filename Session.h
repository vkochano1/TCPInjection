#pragma once

#include <QPSocket.h>
#include <TcpStreamInfo.h>
#include <TCPPacket.h>
#include <EthernetPkt.h>
#include <TcpStreamContext.h>
#include <Utils.h>
#include <TCPStream.h>


class InSocket : public QPSocket
{
 public:
   InSocket(Device& device, const QPSocketCfg& cfg)
    : QPSocket(device, cfg)

   {
   }

  InContext& context() { return context_;}
  InContext context_;
};

class OutSocket : public QPSocket
{
 public:
   OutSocket(Device& device, const QPSocketCfg& cfg)
    : QPSocket(device, cfg)

   {
   }

  OutContext& context() { return context_;}
  OutContext context_;
};

struct AlwaysValid
{
  std::string_view reject() { return std::string_view();}
  constexpr bool validate(const ETH_HDR&) {return true;}
};

class OutStream : public Stream<OutSocket, InSocket, AlwaysValid>
{
public:
  OutStream(OutSocket& out, InSocket& in, AlwaysValid& validator)
     : Stream(out,in, validator)
  {
  }
};

class InStream : public Stream<InSocket, OutSocket, AlwaysValid>
{
public:
  InStream(InSocket& out, OutSocket& in, AlwaysValid& validator)
     : Stream(out,in, validator)
  {
  }
};

struct SessionConfig
{
  SessionConfig(const std::string& localMAC, const std::string& localIP, uint16_t localPort
              , const std::string& targetMAC, const std::string& targetIP, uint16_t targetPort)
              : upstreamCfg_  ("UP   stream ")
              , downstreamCfg_("DOWN stream ")

  {
      target_ = EndPoint {targetMAC,  targetIP, targetPort};
      source_ = EndPoint {localMAC,  localIP, localPort};

      downstreamCfg_.destPort(localPort);
      downstreamCfg_.destMAC(localMAC);
      downstreamCfg_.destIP(localIP);

      upstreamCfg_.sourcePort(targetPort);
      upstreamCfg_.sourceIP(targetIP);
  }

  QPSocketCfg  upstreamCfg_;
  QPSocketCfg downstreamCfg_;
  EndPoint target_;
  EndPoint source_;
};

class Session
{
public:
  Session( Device& device,
           const SessionConfig& sessionCfg)
    : device_(device)
    , in_(device,  sessionCfg.downstreamCfg_)
    , out_(device, sessionCfg.upstreamCfg_)
    , inStream_(in_, out_, valid_)
    , outStream_(out_, in_, valid_)
  {
    in_.context().setTargetEndpoint(sessionCfg.target_);
    LOG(sessionCfg.downstreamCfg_);
    LOG(sessionCfg.upstreamCfg_);
  }

  void init()
  {
     in_.init();
     out_.init();

     in_.open();
     out_.open();
  }

  void poll ()
  {
    inStream_.poll();
    outStream_.poll();
  }

public:
  Device& device_;
  InSocket  in_;
  OutSocket  out_;
  AlwaysValid valid_;
  InStream inStream_;
  OutStream outStream_;
};
