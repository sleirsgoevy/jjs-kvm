#include "gdt.h"
#include "gdt_setup.h"
#include "userspace.h"

asm("farret:\nretf $2");

void farret(unsigned int cs);

unsigned long long the_gdt[SEL_END / 8];

void setup_gdt()
{
    unsigned long long* gdt = the_gdt;
    gdt[SEL_NULL / 8] = 0;
    gdt[SEL_CODE0 / 8] = gdt_entry(0, 0xfffff) | GDT_X;
    gdt[SEL_DATA0 / 8] = gdt_entry(0, 0xfffff);
    gdt[SEL_CODE3 / 8] = gdt_entry(0, 0xfffff) | GDT_X | GDT_RING(3);
    gdt[SEL_DATA3 / 8] = gdt_entry(0, 0xfffff) | GDT_RING(3);
    gdt[SEL_TSS / 8] = tss_segment();
    static struct
    {
        unsigned short sz;
        void* ptr;
    } __attribute__((packed)) gdtr = {SEL_END-1, &the_gdt};
    asm volatile("lgdt %0"::"m"(gdtr));
    unsigned int cs = SEL_CODE0;
    unsigned int ds = SEL_DATA0;
    asm volatile("pushw $8\ncall farret");
    asm volatile("mov %%bx, %%ds\nmov %%bx, %%es\nmov %%bx, %%ss"::"eax"(cs),"ebx"(ds));
}
