#pragma once

#include <QPSocket.h>
#include <TcpStreamInfo.h>
#include <TCPPacket.h>
#include <TCPPacketLite.h>
#include <TcpStreamContext.h>
#include <Utils.h>
#include <TCPStream.h>
#include <unordered_set>
#include <Validators.h>




class OutStream : public Stream<OutContext,AlwaysValid>
{
public:
  OutStream(OutContext& context, AlwaysValid& validator)
     : Stream(context, validator)
  {
  }
};

class InStream : public Stream<InContext, SomeReject>
{
public:
  InStream(InContext& context, SomeReject& validator)
     : Stream(context, validator)
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
    , inContext_(outContext_, in_, out_)
    , outContext_(inContext_, out_, in_)
    , inStream_(inContext_, valid2_)
    , outStream_(outContext_, valid_)
  {
    inContext_.setTargetEndpoint(sessionCfg.target_);
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

  QPSocket  in_;
  QPSocket  out_;

  AlwaysValid valid_;
  SomeReject valid2_;

  InContext inContext_;
  OutContext outContext_;

  InStream inStream_;
  OutStream outStream_;
};
