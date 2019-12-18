#pragma once

#include <sstream>

#pragma pack( push, 1)

struct flags
{
   uint16_t hdrLen () const  { return (ntohs(val_) & 0xF000) >>  12;}

   bool  urg  ()  const   {  return ntohs(val_) & 32;   }
   bool  ack  ()  const   { return ntohs(val_) & 16;    }
   bool  push ()  const   { return ntohs(val_) & 0x8;   }
   bool  rst  ()  const   { return ntohs(val_) & 0x4;   }
   bool  syn  ()  const   { return ntohs(val_) & 0x2;   }
   bool  fin  ()  const   { return ntohs(val_) & 0x1;   }

   uint16_t val_;
};

struct EthernetII
{
  static std::string printMAC(const unsigned char* s)
  {
     std::ostringstream ostrm;

     ostrm << std::hex  << (int)s[0] <<":" <<  (int)s[1] << ":"
                        << (int)s[2]<< ":"  <<(int) s[3] << ":"
                        << (int)s[4]<<":" << (int)s[5];
     return ostrm.str();
  }

  unsigned char dst_mac[6];
  unsigned char src_mac[6];
  uint16_t pkttype;
};

struct IPv4 : public EthernetII
{
  size_t ipHLen() const
  {
    return ph & 0xF;
  }

  static std::string  printIP( const unsigned char* s , uint16_t port)
  {
      std::ostringstream ostrm;
      ostrm << (int) s[0] <<"."  << (int) s[1] <<"."
            << (int) s[2] <<"." << (int) s[3]
            << ":" << ntohs(port);
      return ostrm.str();
  }

  uint8_t  ph;
  uint8_t  _;
  uint16_t totalLen;
  unsigned char h[8];

  unsigned char src_ip[4];
  unsigned char dst_ip[4];
};

struct TCP : public IPv4
{
  char* data()
  {
     return  (char*)  &sp + (int) (f.hdrLen() * 4);
  }

  const char* data() const
  {
     return  (char*)  &sp + (int) (f.hdrLen() * 4);
  }

  uint16_t tcpLen() const
  {
       return  ntohs(totalLen) - ( f.hdrLen() + ipHLen() )  * 4;
  }

  uint16_t winSize() const
  {
       return  ntohs(windowSize);
  }

  uint32_t seqNum() const
  {
      return ntohl(seqNum_);
  }

  uint32_t ackNum() const
  {
      return ntohl(ackNum_);
  }

  void seqNum(uint32_t seqNum)
  {
      seqNum_ = htonl(seqNum);
  }

  void ackNum(uint32_t ackNum)
  {
      ackNum_ = htonl(ackNum);
  }

  std::string_view payload() const
  {
      return std::string_view(data(), tcpLen() );
  }

public:
  uint16_t sp;
  uint16_t dp;

private:
  uint32_t seqNum_;
  uint32_t ackNum_;
public:
  flags f;
  uint16_t windowSize;
  uint16_t checkSum;
  uint16_t urgentPointer;
};

struct TCPPacketLite : public TCP
{
  static TCPPacketLite& fromData(std::string_view& data)
  {
    return *const_cast<TCPPacketLite*> (reinterpret_cast<const TCPPacketLite*> (data.data()));
  }

  friend std::ostream& operator << (std::ostream& ostrm, const TCPPacketLite& pkt)
  {
    //ostrm << "DestMAC:" << printMAC(dst_mac)  << " ,SourceMAC:" << printMAC(src_mac) << std::endl;
    //ostrm << "PktType:" << ntohs(pkttype) << std::endl;
    //ostrm << "TotalLen:" << ntohs(totalLen) << std::endl;
    ostrm << "SourceAddr:" << pkt.printIP(pkt.src_ip, pkt.sp) << " , DestAddr:" << pkt.printIP(pkt.dst_ip, pkt.dp) << std::endl;
    ostrm << "SeqNum: " << pkt.seqNum() << " , AckNum:" << pkt.ackNum() << ", Win " << pkt.winSize() << std::endl;
    //ostrm << "TCPLen: " << tcpLen() << std::endl;
    //ostrm << "IPHLen: " << ipHLen() << std::endl;
    //ostrm << "Flags: SYN(" << f.syn() << ") , FIN("<< f.fin() << ") , ACK(" << f.ack() << ")" << std::endl;
    //ostrm << "HdrLen: " << f.hdrLen() <<  std::endl;

    ostrm << "Data:" << std::string_view(pkt.data(), pkt.tcpLen()) << std::endl;
    return ostrm;
  }
};

#pragma pack(pop)
