#include "gdt.h"

unsigned long long gdt_entry(unsigned int addr, unsigned int sz)
{
#define addr ((unsigned long long)addr)
#define sz ((unsigned long long)sz)
    return (sz & 0xffffu)
         | (addr & 0xffffffu) << 16
         | (sz & 0xf0000u) << 32
         | (addr & 0xff000000u) << 32
         | 0x00c0920000000000ull;
#undef sz
#undef addr
}
