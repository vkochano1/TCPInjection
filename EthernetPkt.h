#pragma once

#pragma pack( push, 1)

struct flags
{
   flags()
   {
   }

   int hdrLen () { return (ntohs(val_) & 0xF000) >>  12;}
   // 6
   bool  urg()   {  return ntohs(val_) & 32;  }
   bool  ack()   { return ntohs(val_) & 16;   }
   bool  push () { return ntohs(val_) & 0x8;  }
   bool  rst ()  { return ntohs(val_) & 0x4;  }
   bool  syn ()  { return ntohs(val_) & 0x2;  }
   bool  fin ()  { return ntohs(val_) & 0x1;  }

   uint16_t val_;
};


struct EthernetII
{
  std::string printMAC(const unsigned char* s)
  {
     std::ostringstream ostrm;

     ostrm << std::hex << (int)s[0] <<":"<<  (int)s[1] << ":"<< (int)s[2]<< ":"  <<(int) s[3] << ":"<< (int)s[4]<<":" << (int)s[5];
     return ostrm.str();
  }

  unsigned char dst_mac[6];
  unsigned char src_mac[6];
  uint16_t pkttype;
};

struct IPv4 : public EthernetII
{
  size_t ipHLen()
  {
    return ph & 0xF;
  }

  std::string  printIP( const unsigned char* s , uint16_t port)
  {
      std::ostringstream ostrm;
      ostrm << (int) s[0] <<"."  << (int) s[1] <<"." << (int) s[2] <<"." << (int) s[3] << ":" << ntohs(port);
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

  size_t tcpLen()
  {
       return  ntohs(totalLen) - ( f.hdrLen() + ipHLen())  * 4;
  }

  uint16_t sp;
  uint16_t dp;

  uint32_t seqNum;
  uint32_t ackNum;

  flags f;
  //uint16_t fl;
  uint16_t windowSize;
  uint16_t checkSum;
  uint16_t urgentPointer;
};


struct ETH_HDR : public TCP
{
  std::string print()
  {
    std::ostringstream ostrm;
    ostrm << "DestMAC:" << printMAC(dst_mac)  << " ,SourceMAC:" << printMAC(src_mac) << std::endl;
    ostrm << "PktType:" << ntohs(pkttype) << std::endl;
    ostrm << "TotalLen:" << ntohs(totalLen) << std::endl;
    ostrm << "SourceAddr:" << printIP(src_ip, sp) << " , DestAddr:" << printIP(dst_ip, dp) << std::endl;
    ostrm << "SeqNum: " << ntohl(seqNum) << " , AckNum:" << ntohl(ackNum) << std::endl;
    ostrm << "TCPLen: " << tcpLen() << std::endl;
    ostrm << "IPHLen: " << ipHLen() << std::endl;
    ostrm << "Flags: SYN(" << f.syn() << ") , FIN("<< f.fin() << ") , ACK(" << f.ack() << ")" << std::endl;
    ostrm << "HdrLen: " << f.hdrLen() <<  std::endl;

    ostrm << std::string(data(), tcpLen()) << std::endl;
    return ostrm.str();
  }
};

#pragma pack(pop)
