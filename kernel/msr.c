#include "msr.h"

unsigned long long read_msr(unsigned int idx)
{
    unsigned int low;
    unsigned int high;
    asm volatile("rdmsr":"=a"(low),"=d"(high):"c"(idx));
    return ((unsigned long long)high) << 32 | low;
}

void write_msr(unsigned int idx, unsigned long long val)
{
    unsigned int low = val;
    unsigned int high = val >> 32;
    asm volatile("wrmsr"::"a"(low),"d"(high),"c"(idx));
}
