#ifndef NDEBUG
#include "memmap.h"
#include "debugout.h"

static char procselfmaps[4096];
static char* psm_p = procselfmaps;

void memmap_log(unsigned int start, unsigned int end, int is_rw, const char* file, unsigned int offset)
{
    for(int i = 28; i >= 0; i -= 4)
        *psm_p++ = "0123456789abcdef"[(start >> i) & 15];
    *psm_p++ = '-';
    for(int i = 28; i >= 0; i -= 4)
        *psm_p++ = "0123456789abcdef"[(end >> i) & 15];
    *psm_p++ = ' ';
    *psm_p++ = 'r';
    if(is_rw)
        *psm_p++ = 'w';
    else
        *psm_p++ = 'o';
    *psm_p++ = ' ';
    for(int i = 0; file[i]; i++)
        *psm_p++ = file[i];
    *psm_p++ = ' ';
    unsigned int offset_a = offset / 10;
    unsigned int offset_b = offset % 10;
    unsigned int printer = 1;
    while(printer <= offset_a)
        printer *= 10;
    while(printer > 1)
    {
        printer /= 10;
        *psm_p++ = '0' + (offset_a / printer) % 10;
    }
    *psm_p++ = '0' + offset_b;
    *psm_p++ = '\n';
}

void memmap_print()
{
    debug_puts(procselfmaps);
}
#endif
