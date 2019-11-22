#include <QPSocketCfg.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>
#include <memory>
#include <arpa/inet.h>
#include <iostream>
#include <sstream>


QPSocketCfg::QPSocketCfg(const std::string& flowName)
{
  flowName_ = flowName;

  numOfRecvBuffers_ = 512;
  numOfSendBuffers_ = 512;

  recvEntrySize_  = 1000;
  sendEntrySize_  = 1000;

  destPort_  =  0;
  destPortMask_= 0;

  sourcePort_ = 0;
  sourcePortMask_ = 0;

  maxInlineSendSize_  = 512;
  portNum_ = 1;
}

void QPSocketCfg::dump(std::ostream& out)
{
    out << "Number of RCV buffers: " << numOfRecvBuffers () << std::endl;
    out << "Number of SND buffers: " << numOfSendBuffers () << std::endl;

    out << "RCV entry size: " << recvEntrySize () << std::endl;
    out << "SND enry size: "  << sendEntrySize () << std::endl;

    out << "DestIP: " << destIP()  << "," << destIPRaw() << std::endl;
    out << "SourceIP: " << sourceIP() << "," << sourceIPRaw() << std::endl;

    out << "DestMAC: " << destMAC()  << std::endl;
    out << "SourceMAC: " << sourceMAC() << std::endl;

    out << "DestPort: " << destPort() << ",  Mask: " << destPortMask() << std::endl;
    out << "SrcPort: " << sourcePort() << ", Mask: " << sourcePortMask() << std::endl;

    out << "MaxInlineSendSize: " << maxInlineSendSize () << std::endl;
    out << "DevicePort: "        << (size_t)portNum () << std::endl;
}


void QPSocketCfg::sourcePort(short sourcePort)
{
  sourcePort_ = sourcePort;
  sourcePortMask_ = 0xFFFF;
}

void QPSocketCfg::destPort(short destPort)
{
  destPort_ = destPort;
  destPortMask_ = 0xFFFF;
}

void QPSocketCfg::sourceIP(const std::string& ip)
{
  ::inet_pton(AF_INET, ip.c_str(), &sourceIPRaw_);
  sourceIP_ = ip;
}

void QPSocketCfg::destIP(const std::string& ip)
{
  ::inet_pton(AF_INET, ip.c_str(), &destIPRaw_);
  destIP_ = ip;
}

void QPSocketCfg::sourceMAC(const std::string& mac)
{
  macFromString(mac, sourceMACRaw_);
  sourceMAC_ = mac;
}

void QPSocketCfg::destMAC(const std::string& mac)
{
  macFromString(mac, destMACRaw_);
  destMAC_ = mac;
}

const std::string& QPSocketCfg::flowName() const
{
  return flowName_;
}

void QPSocketCfg::macFromString(const std::string& strMac,  std::array<uint8_t,6>& bytes)
{
  std::stringstream ss(strMac);
  char unused = 0;
  for ( int i = 0; i < bytes.size(); i++ )
  {
    int tmp;
    ss >> std::hex >> tmp >> unused;
    bytes[i] = static_cast<uint8_t> (tmp);
  }
}

std::ostream& operator << (std::ostream& ostrm , const QPSocketCfg& cfg)
{
   cfg.dump(ostrm);
   return ostrm;
}
