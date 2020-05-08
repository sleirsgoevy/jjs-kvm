#include "idt.h"
#include "gdt_setup.h"
#include "userspace.h"

void setup_idt()
{
    static unsigned long long idt[256];
    for(int i = 0; i < 256; i++)
        idt[i] = idt_entry(SEL_CODE0, (unsigned int)int_unknown, 0);
    idt[0x80] = idt_entry(SEL_CODE0, (unsigned int)int80, 3);
    idt[0x00] = idt_entry(SEL_CODE0, (unsigned int)int00, 0);
    idt[0x06] = idt_entry(SEL_CODE0, (unsigned int)int06, 0);
    idt[0x0e] = idt_entry(SEL_CODE0, (unsigned int)int0e, 0);
    idt[0x68] = idt_entry(SEL_CODE0, (unsigned int)int68, 0);
    struct
    {
        unsigned short sz;
        void* ptr;
    } __attribute__((packed)) idtr = {2047, &idt};
    asm volatile("lidt %0"::"m"(idtr));
}
