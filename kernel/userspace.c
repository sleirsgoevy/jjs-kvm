#include "userspace.h"
#include "gdt.h"
#include "gdt_setup.h"

struct userspace userspace;

static struct
{
    char padding[256];
    unsigned int gp_regs[8];
    unsigned int eflags;
    unsigned int esp;
} kernelspace;

static unsigned int tss[26];

asm("int_entry:\npusha\npush %gs\npush %ds\nmov %ss, %ax\nmov %ax, %ds\nmov %ax, %es\nrdtsc\nadd %eax, userspace\nadc %edx, userspace+4\ncall usreturn\nint_ret:\npop %ds\nmov %ds, %ax\nmov %ax, %es\npop %gs\nmov %gs, %ax\nmov %ax, %fs\nmovl $0, userspace\nmovl $0, userspace+4\nrdtsc\nsub %eax, userspace\nsbb %edx, userspace+4\npopa\nlea 8(%esp), %esp\niret");
void int_ret();

asm(".global int80\nint80:\npushl $0\npushl $0x80\njmp int_entry");
asm(".global int0e\nint0e:\ntestl $3, 8(%esp)\nje int0e_kernel\npushl $0x0e\njmp int_entry\nint0e_kernel:\ncall kernel_page_fault\nlea 4(%esp), %esp\niret");
asm(".global int06\nint06:\npushl $0\npushl $0x06\njmp int_entry");
asm(".global int00\nint00:\npushl $0\npushl $0x00\njmp int_entry");
asm(".global int_unknown\nint_unknown:\nlea 312+userspace, %esp\npushl $0x100\njmp int_entry");
asm(".global int68\nint68:\npushl $0\npushl $0x68\njmp int_entry");
//asm(".global int68\nint68:\nlidt int68idt\nint $0x68\nint68idt:\n.long 0,0");

static char xmm[512];

static void setup_fpu()
{
    asm volatile("mov %cr0, %eax\nand $0xfffffffb, %eax\nor $2, %eax\nmov %eax, %cr0\nmov %cr4, %eax\nor $1536, %eax\nmov %eax, %cr4");
    asm volatile("fninit");
    asm volatile("fxsave %0"::"m"(xmm));
}

void reset_userspace()
{
    asm volatile("fxrstor %0"::"m"(xmm));
    userspace.fnreturn = int_ret;
    userspace.ds = SEL_DATA3;
    userspace.gs = SEL_DATA3;
    for(int i = 0; i < 8; i++)
        userspace.gp_regs[i] = 0;
    userspace.eip = 0;
    userspace.cs = SEL_CODE3;
    userspace.eflags = 514; //IF
    userspace.esp = 0;
    userspace.ss = SEL_DATA3;
}

static struct userspace userspace_bak;
static char xmm_bak[512];

void save_userspace()
{
    asm volatile("fxsave %0"::"m"(xmm_bak));
    userspace_bak = userspace;
    userspace_bak.eip -= 2; // restart the interrupted syscall
}

void restore_userspace()
{
    asm volatile("fxrstor %0"::"m"(xmm_bak));
    userspace = userspace_bak;
}

unsigned long long tss_segment()
{
    return gdt_entry((unsigned int)(void*)tss, sizeof(tss)) & ~GDT_USER & ~GDT_G & ~GDT_W | GDT_X | GDT_A;
}

static void init_tss()
{
    tss[1] = (unsigned int)(1+&userspace);
    tss[2] = (unsigned int)SEL_DATA0;
    asm volatile("ltr %%ax"::"a"(SEL_TSS));
}

void init_userspace()
{
    setup_fpu();
    init_tss();
    reset_userspace();
}

void jump_userspace()
{
    asm volatile("mov %esp, 292+kernelspace\nlea 292+kernelspace, %esp\npushf\npusha\nlea 264+userspace, %esp\nret\nusreturn:\nmov $16, %ax\nmov %ax, %ds\nmov %ax, %es\nlea 256+kernelspace, %esp\npopa\npopf\nmov (%esp), %esp");
}
