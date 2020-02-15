#include "debugout.h"
#include "gdt_setup.h"
#include "paging.h"
#include "userspace.h"
#include "idt_setup.h"
#include "bundle.h"
#include "main.h"
#include "alloc.h"
#include "timer.h"

void _start()
{
    setup_gdt();
    debug_puts("setup_gdt(): ok\n");
    locate_bundle();
    debug_puts("bundle is at 0x");
    debug_putn((unsigned int)bundle, 16);
    debug_putc('\n');
    /*for(volatile unsigned int* p = (volatile unsigned int*)alloc_addr_high; p < (volatile unsigned int*)0x8000000; p++)
        *p = 0xdeadbeefu;*/
    init_paging();
    debug_puts("init_paging(): ok\n");
    init_userspace();
    debug_puts("init_userspace(): ok\n");
    setup_idt();
    debug_puts("setup_idt(): ok\n");
    init_timer();
    debug_puts("init_timer(): ok\n");
    debug_puts("calling main()...\n");
    main();
    debug_puts("main() returned\n");
}
