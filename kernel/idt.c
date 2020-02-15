#include "idt.h"

unsigned long long idt_entry(unsigned short cs, unsigned int eip, unsigned int dpl)
{
    return (eip & 0xffffu)
         | (((unsigned int)cs) << 16)
         | ((dpl & 3ull) << 45)
         | ((eip & 0xffff0000ull) << 32)
         | 0x8e0000000000ull;
}
