#pragma once
#include <tins/utils/resolve_utils.h>
#include <tins/packet_sender.h>
#include <tins/network_interface.h>

namespace Utils
{
  inline uint64_t rdtsc()
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
  }

  inline Tins::HWAddress<6> resolveMAC(const Tins::IPv4Address& ip, const Tins::NetworkInterface& interface)
  {
    Tins::PacketSender sender(interface);
    return Tins::Utils::resolve_hwaddr(interface, ip, sender);
  }

  #define LOG(TEXT)\
    do { std::cerr << TEXT << std::endl;} while(0)

  #define LOG_THROW(EX,TEXT)\
      do { LOG(TEXT); throw EX(TEXT);} while(0)
}
