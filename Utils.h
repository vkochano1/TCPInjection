#pragma once

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


  #define LOG(TEXT)\
    do { std::cerr << TEXT << std::endl;} while(0)

  #define LOG_THROW(EX,TEXT)\
      do { LOG(TEXT); throw EX(TEXT);} while(0)
}
