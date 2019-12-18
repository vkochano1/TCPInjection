#pragma once
#include <iostream>

#include <tins/utils/resolve_utils.h>
#include <tins/packet_sender.h>
#include <tins/network_interface.h>

#define LOG(TEXT)\
  do { std::cerr << TEXT << std::endl;} while(0)

#define LOG_ALWAYS(TEXT)\
    do { std::cerr << TEXT << std::endl;} while(0)

#define LOG_THROW(EX,TEXT)\
    do { LOG(TEXT); throw EX(TEXT);} while(0)
#define LOG(TEXT)

namespace Utils
{
  /*inline uint64_t rdtsc()
  {
     uint32_t lo;

    asm volatile(
      "lfence\n\t"
      "rdtsc\n\t"
      "lfence"
      : "=a"(lo)
      :
      // "memory" avoids reordering. rdx = TSC >> 32.
      : "rdx", "memory");

    return  lo;
  }*/

  inline uint64_t rdtsc()
  {
    uint64_t rax,rdx;
    uint32_t aux;
    asm volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return (rdx << 32) + rax;
  }

  class DummyTimer
  {
  public:
    DummyTimer ()
    {
      nPoints_ = 0;
      avgRecvTime_ = 0;
    }

    void recordStart()
    {
      startRecvTime_ =  Utils::rdtsc();
    }

    void recordEnd()
    {
      endRecvTime_ =  Utils::rdtsc();
      avgRecvTime_ = (avgRecvTime_ * nPoints_ + (endRecvTime_ - startRecvTime_)) / (nPoints_ + 1);

      if (++nPoints_ % 100000 == 0)
      {
        LOG_ALWAYS("Avg ns:" << avgRecvTime_ * 0.25641);
      }
    }

  private:
    size_t startRecvTime_;
    size_t endRecvTime_;
    size_t avgRecvTime_;
    size_t nPoints_;
  };

  inline Tins::HWAddress<6> resolveMAC(const Tins::IPv4Address& ip, const Tins::NetworkInterface& interface)
  {
    Tins::PacketSender sender(interface);
    return Tins::Utils::resolve_hwaddr(interface, ip, sender);
  }


}
