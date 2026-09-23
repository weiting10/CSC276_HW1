#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>


/***32-bit time stamp***/
static inline uint32_t rdtscp()
{
    uint32_t rv;
    asm volatile("rdtscp" : "=a"(rv)::"edx", "ecx");
    return rv;
}

/***64-bit time stamp***/
static inline uint64_t rdtscp64()
{
    uint32_t low, high;
    asm volatile("rdtscp" : "=a" (low), "=d" (high) :: "ecx");
    return (((uint64_t)high) << 32) | low;
}

/***measure access latency***/
static inline uint32_t memaccesstime(void *v) {
  uint32_t rv;
  asm volatile("mfence\n"
               "lfence\n"
               "rdtscp\n"
               "mov %%eax, %%esi\n"
               "mov (%1), %%eax\n"
               "rdtscp\n"
               "sub %%esi, %%eax\n"
               : "=&a"(rv)
               : "r"(v)
               : "ecx", "edx", "esi");
  return rv;
}

/***access an address***/
static inline void maccess(void *p) {
  asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax");
}

/***flush***/
static void flush(void *p) {
  asm volatile("clflush 0(%0)\n" : : "c"(p) : "rax");
}
