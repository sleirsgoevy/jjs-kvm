#pragma once

void setup_gdt();

enum gdt_sel
{
    SEL_NULL = 0,
    SEL_CODE0 = 8,
    SEL_DATA0 = 16,
    SEL_CODE3 = 27,
    SEL_DATA3 = 35,
    SEL_TSS = 43,
    SEL_TLS = 51,
    SEL_END = 56
};

extern unsigned long long the_gdt[];
