#pragma once

#include <tins/ethernetII.h>
#include <tins/tins.h>
#include <arpa/inet.h>

struct EndPoint
{
  using MAC = Tins::HWAddress<6>;
  using IP = Tins::IPv4Address;
  using Port = uint16_t;

  MAC outMac_;
  IP  outIP_;
  Port outPort_;
};

class TcpStreamInfo
{
public:
    using MAC = Tins::HWAddress<6>;
    using IP = Tins::IPv4Address;
    using Port = uint16_t;

    TcpStreamInfo()
    {
      sourcePort_ = 0;
      destPort_   = 0;
      sourcePortRaw_ = 0;
      destPortRaw_ = 0;

      std::memset(&sourceMACRaw_[0], 0, sizeof(sourceMACRaw_));
      std::memset(&destMACRaw_[0], 0, sizeof(destMACRaw_));
    }

    void init(const MAC& s_mac, const MAC& d_mac
            , const IP& s_ip, const IP& d_ip
            , const Port s_p, const Port d_p )
    {
      sourceMAC_ = s_mac;
      destMAC_ = d_mac;

      sourceIP_ = s_ip;
      destIP_ = d_ip;

      sourcePort_ = s_p;
      sourcePortRaw_ = htons(sourcePort_);

      destPort_ = d_p;
      destPortRaw_ = htons(destPort_);

      {
        std::memcpy(&sourceMACRaw_[0], sourceMAC_.begin(),  sizeof(sourceMACRaw_));
      }

      {

        std::memcpy(&destMACRaw_[0], destMAC_.begin(), sizeof(destMACRaw_));
      }

      {
        sourceIPRaw_ = sourceIP_;
      }

      {
        destIPRaw_ = destIP_;
      }
    }

public:
    void fillSourceIP(unsigned char* buf) const
    {
       *reinterpret_cast<uint32_t*>(buf)  = sourceIPRaw_;
    }

    void fillDestIP(unsigned char* buf) const
    {
      *reinterpret_cast<uint32_t*>(buf)  = destIPRaw_;
    }

    void fillSourceMAC(unsigned char* buf) const
    {
       std::memcpy(buf, &sourceMACRaw_[0], sizeof(sourceMACRaw_));
    }

    void fillDestMAC(unsigned char* buf) const
    {
      std::memcpy(buf, &destMACRaw_[0], sizeof(destMACRaw_));
    }

    void fillSourcePort(uint16_t& sp) const
    {
      sp = sourcePortRaw_;
    }

    void fillDestPort(uint16_t& dp) const
    {
      dp = destPortRaw_;
    }

public:

    const IP& sourceIP() const  { return (sourceIP_);}
    const IP& destIP() const  { return (destIP_);}

    const MAC& sourceMAC() const  { return (sourceMAC_);}
    const MAC& destMAC() const  { return (destMAC_);}

    const Port& sourcePort() const  { return (sourcePort_);}
    const Port& destPort() const  { return (destPort_);}

private:
    char sourceMACRaw_[6];
    MAC sourceMAC_;

    char destMACRaw_[6];
    MAC destMAC_;

    uint32_t sourceIPRaw_;
    Tins::IPv4Address sourceIP_;

    uint32_t destIPRaw_;
    Tins::IPv4Address destIP_;

    uint16_t sourcePort_;
    uint16_t destPort_;

    uint16_t sourcePortRaw_;
    uint16_t destPortRaw_;
};
