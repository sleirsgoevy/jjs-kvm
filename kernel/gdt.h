#pragma once

unsigned long long gdt_entry(unsigned int addr, unsigned int sz);

enum
{
    GDT_RING_SHIFT = 45,
    GDT_BIT_USER = 44, //set by default
    GDT_BIT_X = 43,
    GDT_BIT_W = 41, //set by default
    GDT_BIT_G = 55, //set by default
    GDT_BIT_A = 40,
    GDT_BIT_32 = 54, //set by default
};

#define GDT_RING(x) (((unsigned long long)(x)) << GDT_RING_SHIFT)
#define GDT_USER (1ull << GDT_BIT_USER)
#define GDT_X (1ull << GDT_BIT_X)
#define GDT_W (1ull << GDT_BIT_W)
#define GDT_G (1ull << GDT_BIT_G)
#define GDT_A (1ull << GDT_BIT_A)
#define GDT_32 (1ull << GDT_BIT_32)
