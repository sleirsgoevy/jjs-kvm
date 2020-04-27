#pragma once

struct userspace
{
    unsigned long long delta;
    char padding[256];
    void* fnreturn;
    unsigned int ds;
    unsigned int gs;
    unsigned int gp_regs[8];
    int exit_reason;
    unsigned int errorcode;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int esp;
    unsigned int ss;
};

extern struct userspace userspace;

enum
{
    GP_REG_EAX = 7,
    GP_REG_ECX = 6,
    GP_REG_EDX = 5,
    GP_REG_EBX = 4,
    GP_REG_ESP = 3,
    GP_REG_EBP = 2,
    GP_REG_ESI = 1,
    GP_REG_EDI = 0
};

//call once to set up the relevant structures
void init_userspace();

//call to reset for a fresh run
void reset_userspace();

//actually transfer control to userspace
void jump_userspace();

//back up the registers
void save_userspace();

//restore the registers
void restore_userspace();

//for GDT setup code
unsigned long long tss_segment();

//for IDT setup code
void int80(); //syscall
void int0e(); //unknown
void int68(); //PIT
void int_unknown();
