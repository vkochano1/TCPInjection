#pragma once

#include <string>
#include <array>

struct QPSocketCfg
{
public:
    QPSocketCfg(const std::string& flowName = "");

public:
    size_t      numOfRecvBuffers () const  { return  numOfRecvBuffers_;};
    size_t      numOfSendBuffers () const  { return  numOfSendBuffers_;};

    size_t      recvEntrySize () const {return recvEntrySize_;}
    size_t      sendEntrySize () const {return sendEntrySize_;}

    //receive filters
    short destPort () const { return destPort_;};
    short destPortMask() const {return destPortMask_;};

    short sourcePort() const {return sourcePort_;}
    short sourcePortMask()  const {return sourcePortMask_;}

    const std::string& sourceIP() const { return sourceIP_;}
    const std::string& destIP() const { return destIP_;}

    uint32_t sourceIPRaw() const { return sourceIPRaw_;}
    uint32_t destIPRaw() const { return destIPRaw_;}

    short portNum() const {return portNum_;}
    size_t  maxInlineSendSize () const {return maxInlineSendSize_;}

    decltype(auto) sourceMACRaw() const  { return  (sourceMACRaw_);}
    decltype(auto) destMACRaw()  const   { return  (destMACRaw_);}
    decltype(auto) sourceMAC() const  { return  (sourceMAC_);}
    decltype(auto) destMAC()  const   { return  (destMAC_);}

    void dump(std::ostream& out) const;
    friend std::ostream& operator << (std::ostream& , const QPSocketCfg& cfg);

public:
  void sourcePort(short sourcePort);
  void destPort(short destPort);
  void sourceIP(const std::string& ip);
  void destIP(const std::string& ip);
  void sourceMAC(const std::string& mac);
  void destMAC(const std::string& mac);
  const std::string& flowName() const;

private:
  static void macFromString(const std::string& strMac,  std::array<uint8_t,6>& bytes);

private:
    std::string flowName_;
    short        portNum_;
    size_t      numOfRecvBuffers_;
    size_t      numOfSendBuffers_;

    size_t      recvEntrySize_;
    size_t      sendEntrySize_;

    //receive filters
    short destPort_;
    short destPortMask_;

    short sourcePort_;
    short sourcePortMask_;

    std::string sourceIP_;
    uint32_t  sourceIPRaw_;

    std::string destIP_;
    uint32_t destIPRaw_;

    std::string sourceMAC_;
    std::array<uint8_t,6> sourceMACRaw_;

    std::string destMAC_;
    std::array<uint8_t,6> destMACRaw_;

    size_t maxInlineSendSize_;
};
