#pragma once

// print character
void debug_putc(char c);

// print NULL-terminated string
void debug_puts(char* s);

// print unsigned number with given base
void debug_putn(unsigned long long n, int base);

// print signed number with given base
void debug_putns(long long n, int base);

#ifdef NDEBUG
//we still want to ensure that all arguments do get evaluated
#define debug_putc(c) ((c), 0)
#define debug_puts(s) ((s), 0)
#define debug_putn(n, base) ((n), (base), 0)
#define debug_putns(n, base) ((n), (base), 0)
#endif
