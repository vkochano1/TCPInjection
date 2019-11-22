#pragma once

#include <tins/ethernetII.h>
#include <tins/tins.h>
#include <iostream>

class TCPDumper
{
public:
  TCPDumper(const char* buf, size_t len)
   : ether_(buf, len)
   , tcp_( ether_.rfind_pdu<Tins::TCP>())
   , ip_ ( ether_.rfind_pdu<Tins::IP> ())
  {
    data_ = tcp_.find_pdu<Tins::RawPDU>();
  }

  void dump(std::ostream& ostrm)
  {
    ostrm << "ETH: " <<  " Source:" << ether_.src_addr()
                      << ",Dest:"   << ether_.dst_addr()
                      << std::endl;

    ostrm << "TCP: " << " Source:" << ip_.src_addr() << ":" << tcp_.sport()
		      << ",Dest:"   << ip_.dst_addr() << ":" << tcp_.dport()
          << ",Flags:"  << "S" << (bool)tcp_.get_flag(Tins::TCP::Flags::SYN)
                                   << "A" << (bool)tcp_.get_flag(Tins::TCP::Flags::ACK)
                                   << "F" << (bool)tcp_.get_flag(Tins::TCP::Flags::FIN)
                                   << "R" << (bool)tcp_.get_flag(Tins::TCP::Flags::RST)
           << "Seq:"     << "seq:" <<tcp_.seq() <<"," << "ack:" << tcp_.ack_seq();
   if(data_)
   {
     ostrm << "Payload: " << std::string((const char*) &data_->payload()[0], data_->payload_size());
   }

   ostrm << std::endl;
  }

  const Tins::EthernetII& ether () const {return ether_;};
  const Tins::TCP&        tcp() const {return tcp_;};
  const Tins::IP&         ip() const {return ip_;}

public:

  Tins::EthernetII ether_;
  Tins::TCP&        tcp_;
  Tins::IP&         ip_;
  Tins::RawPDU*     data_;
};

class TCPPacket
{
public:
  TCPPacket()
  {
    data_ = new Tins::RawPDU("");
    ip_ = new Tins::IP();

    ip_ ->flags(Tins::IP::Flags::DONT_FRAGMENT);
    tcp_ = new Tins::TCP();
    tcp_->flags(Tins::TCP::Flags::PSH | Tins::TCP::Flags::ACK);

    tcp_->inner_pdu(data_);
    ip_->inner_pdu(tcp_);
    pkt_.inner_pdu(ip_);
  }

  TCPPacket(TCPDumper& dumper)
  {
    data_ = new Tins::RawPDU("");
    ip_ = new Tins::IP(dumper.ip());
    tcp_ = new Tins::TCP(dumper.tcp());
    //cp_->flags(Tins::TCP::Flags::PSH);

    tcp_->inner_pdu(data_);
    ip_->inner_pdu(tcp_);
    pkt_.inner_pdu(ip_);
  }

  void setSrcMAC (const std::string& addr)
  {
    Tins::HWAddress<6> mac(addr);
    pkt_.src_addr(mac);
  }

  void setDstMAC (const std::string& addr)
  {
    Tins::HWAddress<6> mac(addr);
    pkt_.dst_addr(mac);
  }

  void setSrcIP(const std::string& addr)
  {
    Tins::IPv4Address ipv4 (addr);
    ip_->src_addr(ipv4);
  }

  void setDstIP(const std::string& addr)
  {
    Tins::IPv4Address ipv4 (addr);
    ip_->dst_addr(ipv4);
  }

  void setSrcPort(uint16_t port)
  {
    tcp_->sport(port);
  }

  void setDstPort(uint16_t port)
  {
    tcp_->dport(port);
  }

  void setData(const char* buf, size_t len)
  {
    data_->payload(buf, buf + len);
  }

  void setAckSeq(size_t num)
  {
    tcp_->ack_seq(num);
  }

  void setSeq(size_t num)
  {
    tcp_->seq(num);
  }

  Tins::IP& ip() {return *ip_;}
  Tins::TCP& tcp() {return *tcp_;}

  Tins::EthernetII& ether() {return pkt_;}
  Tins::EthernetII pkt_;
  Tins::IP* ip_;
  Tins::TCP* tcp_;
  Tins::RawPDU* data_;
};
