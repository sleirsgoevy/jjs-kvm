#include "timer.h"
#include "userspace.h"
#include "gdt_setup.h"
#include "debugout.h"

asm("do_hlt:\nhlt");
void do_hlt();

unsigned long long tsc_per_us;

unsigned int wait_interrupt()
{
    reset_userspace();
    userspace.exit_reason = 0;
    userspace.eip = (unsigned int)do_hlt; //not actually userspace
    userspace.cs = SEL_CODE0; //ring 0
    userspace.eflags = 514; //IF
    jump_userspace(); //wait for interrupt
    return userspace.exit_reason;
}

void wait_for_interrupt(unsigned int which)
{
    unsigned int q;
    while((q = wait_interrupt()) != which)
    {
        debug_puts("exit_reason = 0x");
        debug_putn(q, 16);
        debug_putc('\n');
    }
}

void init_timer()
{
#define outb(to, from) asm volatile("outb %%al, $"#to"\n.byte 0xeb, 0, 0xeb, 0"::"a"(from))
    // PIC & PIT setup copy-pasted from SeaBIOS
    // allows jumping to jjs-kvm straight after CPU reset
    outb(0x20, 0x11);
    outb(0xa0, 0x11);
    outb(0x21, 0x68);
    outb(0xa1, 0x70);
    outb(0x21, 4);
    outb(0xa1, 2);
    outb(0x21, 1);
    outb(0xa1, 1);
    outb(0x21, 0xfe);
    outb(0x43, 0x34);
    outb(0x40, 0);
    outb(0x40, 0);
#undef outb
    unsigned long long counter = 0;
    for(int i = 0; i < 10; i++)
    {
        union
        {
            unsigned long long tsc;
            struct
            {
                unsigned int low;
                unsigned int high;
            };
        } tsc_u;
        wait_for_interrupt(0x68);
        asm volatile("outb %%al, $0x20"::"a"(0x20));
        asm volatile("rdtsc":"=d"(tsc_u.high),"=a"(tsc_u.low));
        unsigned long long tsc0 = tsc_u.tsc;
        wait_for_interrupt(0x68);
        asm volatile("outb %%al, $0x20"::"a"(0x20));
        asm volatile("rdtsc":"=d"(tsc_u.high),"=a"(tsc_u.low));
        counter += tsc_u.tsc - tsc0;
    }
    tsc_per_us = counter / 549254;
    reset_userspace();
    debug_putn(tsc_per_us, 10);
    debug_puts(" TSC ticks per microsecond\n");
}
