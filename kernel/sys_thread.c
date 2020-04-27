#include "sys_thread.h"
#include "gdt.h"
#include "gdt_setup.h"
#include "vmem.h"
#include <asm/ldt.h>
#include <errno.h>

static unsigned long long tls_bak;

void save_thread_area()
{
    tls_bak = the_gdt[SEL_TLS / 8];
}

void restore_thread_area()
{
    the_gdt[SEL_TLS / 8] = tls_bak;
}

unsigned int sys_set_thread_area(unsigned int udesc, unsigned int _1, unsigned int _2, unsigned int _3, unsigned int _4)
{
    if(check_writable((void*)udesc, sizeof(struct user_desc)) < 0)
        return -EFAULT;
    do_cow((void*)udesc, sizeof(struct user_desc));
    struct user_desc* ud = (struct user_desc*)udesc;
    if(ud->seg_not_present)
    {
        if(ud->entry_number != SEL_TLS / 8)
            return -EINVAL;
        the_gdt[ud->entry_number] = 0;
        return 0;
    }
    if(ud->entry_number == -1)
        ud->entry_number = SEL_TLS / 8;
    else if(ud->entry_number != SEL_TLS / 8 || the_gdt[ud->entry_number])
        return -EINVAL;
    unsigned long long descr = gdt_entry(ud->base_addr, ud->limit);
    if(!ud->limit_in_pages)
        descr &= ~GDT_G;
    descr |= GDT_RING(3);
    the_gdt[ud->entry_number] = descr;
    return 0;
}
