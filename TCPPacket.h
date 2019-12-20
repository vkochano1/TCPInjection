#pragma once

#include <tins/ethernetII.h>
#include <tins/tins.h>
#include <iostream>
#include <TcpStreamInfo.h>

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

  TCPPacket(std::string_view& data)
   : pkt_((uint8_t*) (data.data()), data.size())
   , tcp_( &pkt_.rfind_pdu<Tins::TCP>())
   , ip_ ( &pkt_.rfind_pdu<Tins::IP> ())
  {
    data_ = tcp_->find_pdu<Tins::RawPDU>();
  }

  void setSource (const EndPoint& endPoint)
  {
    pkt_.src_addr(endPoint.mac());
    ip_->src_addr(endPoint.ip());
    tcp_->sport(endPoint.port());
  }

  void setDest (const EndPoint& endPoint)
  {
    pkt_.dst_addr(endPoint.mac());
    ip_->dst_addr(endPoint.ip());
    tcp_->dport(endPoint.port());
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

  void dump(std::ostream& ostrm)
  {
      ostrm << "ETH: "   <<  " Source:" << pkt_.src_addr()
                         << ",Dest:"   << pkt_.dst_addr()
                         << std::endl;

      ostrm << "TCP: "    << " Source:" << ip_->src_addr() << ":" << tcp_->sport()
  		      << ",Dest:"   << ip_->dst_addr() << ":" << tcp_->dport()
            << ",Flags:"  << "S" << (bool)tcp_->get_flag(Tins::TCP::Flags::SYN)
                          << "A" << (bool)tcp_->get_flag(Tins::TCP::Flags::ACK)
                          << "F" << (bool)tcp_->get_flag(Tins::TCP::Flags::FIN)
                          << "R" << (bool)tcp_->get_flag(Tins::TCP::Flags::RST)
             << "Seq:"  << tcp_->seq() <<"," << "ackSeq:" << tcp_->ack_seq();

     if(data_)
     {
       ostrm << "Payload: " << std::string_view(reinterpret_cast<const char*> (&data_->payload()[0]), data_->payload_size());
     }

     ostrm << std::endl;
  }

  Tins::IP& ip() {return *ip_;}
  Tins::TCP& tcp() {return *tcp_;}
  Tins::EthernetII& ether() {return pkt_;}

private:
  Tins::EthernetII pkt_;
  Tins::IP* ip_;
  Tins::TCP* tcp_;
  Tins::RawPDU* data_;
};
