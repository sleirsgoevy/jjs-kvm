#include "debugout.h"

#ifdef NDEBUG

#undef debug_putc
void debug_putc(char c){}
#undef debug_puts
void debug_puts(char* s){}
#undef debug_putn
void debug_putn(unsigned long long n, int base){}
#undef debug_putns
void debug_putns(long long n, int base){}

#else

void debug_putc(char c)
{
    unsigned int eax = (unsigned char)c;
    asm volatile("out %%al, $0xe9"::"eax"(eax));
}

void debug_puts(char* s)
{
    for(int i = 0; s[i]; i++)
        debug_putc(s[i]);
}

static void _debug_putd(int d)
{
    if(d < 10)
        debug_putc('0'+d);
    else
        debug_putc('a'+d-10);
}

static void _debug_putn(long long n, int base)
{
    long long q = 1;
    while(n >= q)
        q *= base;
    while(q > 1)
    {
        q /= base;
        _debug_putd(n/q);
        n %= q;
    }
}

void debug_putn(unsigned long long n, int base)
{
    _debug_putn(n/base/base, base);
    if(n >= base * base)
        _debug_putd(n/base%base);
    else
        _debug_putn(n/base%base, base);
    _debug_putd(n%base);
}

void debug_putns(long long n, int base)
{
    if(n < 0)
    {
        debug_putc('-');
        n = -n;
    }
    debug_putn(n, base);
}

#endif
