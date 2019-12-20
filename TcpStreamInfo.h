#pragma once

#include <tins/ethernetII.h>
#include <tins/tins.h>
#include <arpa/inet.h>

struct EndPoint
{
public:
  using MAC = Tins::HWAddress<6>;
  using IP = Tins::IPv4Address;
  using Port = uint16_t;

public:
  EndPoint ()
  {
    std::memset(&rawMAC_[0], 0, sizeof(rawMAC_));
    rawPort_ = 0;
    rawIP_;
  }

  EndPoint(const MAC& mac, const IP& ip, const Port& port)
  {
    init(mac, ip, port);
  }

  void init(const MAC& mac, const IP& ip, const Port& port)
  {
    mac_ = mac;
    std::memcpy(&rawMAC_[0], mac_.begin(),  sizeof(rawMAC_));

    port_ = port;
    rawPort_ = htons(port);

    ip_ = ip;
    rawIP_ = ip;
  }

  const IP& ip() const
  {
    return ip_;
  }

  const Port& port() const
  {
    return port_;
  }

  const MAC& mac() const
  {
    return mac_;
  }

  void fillIP(unsigned char* buf) const
  {
     *reinterpret_cast<uint32_t*>(buf)  = rawIP_;
  }

  void fillMAC(unsigned char* buf) const
  {
     std::memcpy(buf, &rawMAC_[0], sizeof(rawMAC_));
  }

  void fillPort(uint16_t& port) const
  {
    port = rawPort_;
  }

private:
  char rawMAC_[6];
  uint16_t rawPort_;
  uint32_t rawIP_;

private:
  MAC mac_;
  IP  ip_;
  Port port_;
};

class TcpStreamInfo
{
public:
    TcpStreamInfo()
    {
    }

    void init(const EndPoint& source, const EndPoint& dest)
    {
      source_ = source;
      dest_ = dest;
    }

    const EndPoint& source() const  { return (source_);}
    const EndPoint& dest  () const  { return (dest_);}

private:
  EndPoint source_;
  EndPoint dest_;
};
