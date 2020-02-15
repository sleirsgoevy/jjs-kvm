#include "runtime.h"
#include "userspace.h"
#include "paging.h"
#include "fs.h"
#include "sys_fs.h"
#include "sys_vmem.h"
#include "sys_thread.h"
#include "sys_mock.h"
#include "debugout.h"
#include "alloc.h"
#include "timer.h"
#include <signal.h>
#include <errno.h>
#include <sys/syscall.h>

void reset_runtime()
{
    reset_sys_fs();
}

static syscall_t* syscalls[] = {
    [__NR_read] = sys_read,
    [__NR_write] = sys_write,
    [__NR_close] = sys_close,
    [__NR_brk] = sys_brk,
    [__NR_set_thread_area] = sys_set_thread_area,
    [__NR_uname] = sys_uname,
    [__NR_readlink] = sys_readlink,
    [__NR_mmap2] = sys_mmap2,
    [__NR_rt_sigprocmask] = sys_rt_sigprocmask,
    [__NR_getpid] = sys_getpid,
    [__NR_gettid] = sys_gettid,
    [__NR_access] = sys_access,
    [__NR_fstat64] = sys_fstat64,
    [__NR_mremap] = sys_mremap,
    [__NR_munmap] = sys_munmap,
    [__NR__llseek] = sys_llseek
};

enum{BAILOUT = 0x80000000, ML = 0x80000001, TL = 0x80000002};

int main_loop(unsigned long long tl)
{
    tl *= tsc_per_us;
    while(1)
    {
        jump_userspace();
        debug_puts("userspace delta: ");
        debug_putn(userspace.delta, 10);
        debug_puts(" ticks\n");
        debug_puts("tl = ");
        debug_putn(tl, 10);
        debug_putc('\n');
        if(tl < userspace.delta)
            return TL;
        tl -= userspace.delta;
        switch(userspace.exit_reason)
        {
        case 0x0e: //page fault
            debug_puts("main_loop(): page fault in userspace (errorcode=0x");
            debug_putn(userspace.errorcode, 16);
            debug_puts(")\n");
            if(userspace.errorcode == 7) // probably CoW
            {
                unsigned int fault_addr;
                asm volatile("mov %%cr2, %0":"=r"(fault_addr));
                fault_addr &= ~4095u;
                unsigned int cowed = cow_range(fault_addr, fault_addr+4096u);
                if(cowed > 1)
                    debug_puts("main_loop(): error: cowed > 1\n");
                else if(cowed == 1)
                    continue;
            }
            return -SIGSEGV;
        case 0x80: //syscall
        {
            unsigned int syscall_no = userspace.gp_regs[GP_REG_EAX];
            if(syscall_no == __NR_exit || syscall_no == __NR_exit_group)
            {
                debug_puts("main_loop(): exit(");
                debug_putn(userspace.gp_regs[GP_REG_EBX], 10);
                debug_puts(")\n");
                return userspace.gp_regs[GP_REG_EBX];
            }
            debug_puts("sys_");
            debug_putn(userspace.gp_regs[GP_REG_EAX], 10);
            debug_puts("(");
            debug_putn(userspace.gp_regs[GP_REG_EBX], 10);
            debug_puts(", ");
            debug_putn(userspace.gp_regs[GP_REG_ECX], 10);
            debug_puts(", ");
            debug_putn(userspace.gp_regs[GP_REG_EDX], 10);
            debug_puts(", ");
            debug_putn(userspace.gp_regs[GP_REG_ESI], 10);
            debug_puts(", ");
            debug_putn(userspace.gp_regs[GP_REG_EDI], 10);
            debug_puts(")\n");
            if(syscall_no >= sizeof(syscalls)/sizeof(*syscalls) || !syscalls[syscall_no])
            {
                debug_puts("main_loop(): unknown syscall ");
                debug_putn(userspace.gp_regs[GP_REG_EAX], 10);
                debug_putc('\n');
                return BAILOUT;
            }
            unsigned int ans = syscalls[syscall_no](userspace.gp_regs[GP_REG_EBX], userspace.gp_regs[GP_REG_ECX], userspace.gp_regs[GP_REG_EDX], userspace.gp_regs[GP_REG_ESI], userspace.gp_regs[GP_REG_EDI]);
            debug_puts(" = ");
            debug_putns((int)ans, 10);
            debug_putc('\n');
            if(ml == ML_EXCEEDED)
                return ML;
            if(ans == -ENOSYS)
                return BAILOUT;
            userspace.gp_regs[GP_REG_EAX] = ans;
            continue;
        }
        case 0x68:
            asm volatile("outb %%al, $0x20"::"a"(0x20));
            break;
        default: //unknown
            debug_puts("main_loop(): unknown exit_reason 0x");
            debug_putn(userspace.exit_reason, 16);
            debug_puts(", terminating\n");
            return -SIGILL;
        }
    }
}
