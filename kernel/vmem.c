#include "vmem.h"
#include "paging.h"
#include "userspace.h"
#include "debugout.h"
#include "elf.h"
#include "memmap.h"
#include "done.h"
#include <errno.h>
#include <bits/auxv.h>

static unsigned int cur_brk;
static unsigned int orig_brk;
static const char* the_exe;

int load_executable(struct file* fs, struct fd* f, struct entry* ans, const char* exe, const char** argv, const char** envp)
{
    unsigned int last_addr;
    unsigned int phdr_addr;
    unsigned int phnum;
    unsigned int at_base = 0;
    memmap_log(0xbf800000u, 0xc0000000u, 1, "stack", 0);
    map_range(0xbf800000u, 0xc0000000u, 1); //stack
    char interp[256];
    unsigned int main_base;
    unsigned int entry = load_elf(f, &last_addr, &phdr_addr, &phnum, &main_base, interp, 256);
    unsigned int at_entry = entry;
    debug_puts("entry = 0x");
    debug_putn(entry, 16);
    debug_puts(", base = 0x");
    debug_putn(main_base, 16);
    debug_putc('\n');
    if(!entry)
        return -ENOEXEC;
    if(*interp)
    {
        debug_puts("dynamic executable!\n");
        struct fd dyld;
        int status = fs_open(&dyld, fs, interp, 0);
        if(status)
        {
            debug_puts("load_executable: fs_open(dyld) failed: ");
            debug_putns(status, 10);
            debug_puts("\n");
            return status;
        }
        unsigned int dyld_la, dyld_pa;
        entry = load_elf(&dyld, &dyld_la, &dyld_pa, 0, &at_base, 0, 0);
        debug_puts("at_base = 0x");
        debug_putn(at_base, 16);
        debug_putc('\n');
        if(!entry)
            return -ENOEXEC;
    }
    the_exe = exe;
    cur_brk = orig_brk = (last_addr + 0x2000fffu) & ~4095u;
    for(volatile unsigned int* p = (volatile unsigned int*)0xbf800000; p < (volatile unsigned int*)0xc0000000; p++)
        *p = 0;
    unsigned int esp = 0xc0000000;
    //auxv strings
    esp -= 5; //"i686"
    esp -= 16; //"notrandomdata!!!"
    for(int i = 0; exe[i]; i++)
        esp--;
    esp--; //null
    //argv
    for(int i = 0; argv[i]; i++)
    {
        for(int j = 0; argv[i][j]; j++)
            esp--;
        esp--; //null
    }
    //envp
    for(int i = 0; envp[i]; i++)
    {
        for(int j = 0; envp[i][j]; j++)
            esp--;
        esp--; //null
    }
    unsigned int strp = esp;
    esp &= ~3u;
    esp -= 19 * 8; //auxv+null
    for(int i = 0; argv[i]; i++)
        esp -= 4;
    esp -= 4; //null
    for(int i = 0; envp[i]; i++)
        esp -= 4;
    esp -= 4; //null
    esp -= 4; //argc
    esp &= ~15u; // required by cdecl
    unsigned int* esp_out = (unsigned int*)esp;
    char* strp_out = (char*)strp;
    for(*esp_out = 0; argv[*esp_out]; ++*esp_out);
    esp_out++;
    for(int i = 0; argv[i]; i++)
    {
        *esp_out++ = (unsigned int)strp_out;
        for(int j = 0; argv[i][j]; j++)
            *strp_out++ = argv[i][j];
        *strp_out++ = 0;
    }
    *esp_out++ = 0;
    for(int i = 0; envp[i]; i++)
    {
        *esp_out++ = (unsigned int)strp_out;
        for(int j = 0; envp[i][j]; j++)
            *strp_out++ = envp[i][j];
        *strp_out++ = 0;
    }
    *esp_out++ = 0;
    //auxv
    *esp_out++ = AT_HWCAP;
    *esp_out++ = 0xbfebfbff; // wtf is this? cpuid??
    *esp_out++ = AT_PAGESZ;
    *esp_out++ = 4096;
    *esp_out++ = AT_CLKTCK;
    *esp_out++ = 100;
    *esp_out++ = AT_PHDR;
    *esp_out++ = (unsigned int)phdr_addr;
    *esp_out++ = AT_PHENT;
    *esp_out++ = 32;
    *esp_out++ = AT_PHNUM;
    *esp_out++ = phnum;
    *esp_out++ = AT_BASE;
    *esp_out++ = at_base;
    *esp_out++ = AT_FLAGS;
    *esp_out++ = 0;
    *esp_out++ = AT_ENTRY;
    *esp_out++ = (unsigned int)at_entry;
    *esp_out++ = AT_UID;
    *esp_out++ = 179;
    *esp_out++ = AT_EUID;
    *esp_out++ = 179;
    *esp_out++ = AT_GID;
    *esp_out++ = 179;
    *esp_out++ = AT_EGID;
    *esp_out++ = 179;
    *esp_out++ = AT_SECURE;
    *esp_out++ = 0;
    *esp_out++ = AT_RANDOM;
    *esp_out++ = (unsigned int)strp_out;
    for(int i = 0; i < 16; i++)
        *strp_out++ = "notrandomdata!!!"[i];
    *esp_out++ = AT_HWCAP2;
    *esp_out++ = 0;
    *esp_out++ = AT_EXECFN;
    *esp_out++ = (unsigned int)strp_out;
    for(int i = 0; exe[i]; i++)
        *strp_out++ = exe[i];
    *strp_out++ = 0;
    *esp_out++ = AT_PLATFORM;
    *esp_out++ = (unsigned int)strp_out;
    for(int i = 0; i < 5; i++)
        *strp_out++ = "i686\0"[i];
    if(((unsigned int)strp_out) != 0xc0000000u)
    {
        debug_puts("load_executable: error: strp_out underrun/overrun");
        return -EFAULT;
    }
    ans->eip = entry;
    ans->esp = esp;
}

void setup_entry_regs(struct entry* entry)
{
    reset_userspace();
    userspace.eip = entry->eip;
    userspace.esp = entry->esp;
    cur_brk = orig_brk;
}

unsigned int brk(unsigned int x)
{
    unsigned int old = (cur_brk + 4095u) & ~4095u;
    unsigned int new = (x + 4095u) & ~4095u;
    if(new < orig_brk)
        return cur_brk;
    if(new > old)
    {
        if(check_access(old, new, -1) != new)
            return cur_brk;
        memmap_log(old, new, 1, "heap", 0);
        map_range(old, new, 1);
        for(volatile unsigned int* p = (volatile unsigned int*)old; p < (volatile unsigned int*)new; p++)
            *p = 0;
        return cur_brk = x;
    }
    else if(new < old)
    {
        unmap_range(new, old);
        return cur_brk = x;
    }
}

void kernel_page_fault()
{
    done(-11); // report error on all tests
    debug_puts("FATAL: in-kernel page fault!\n");
    memmap_print(); // hope it doesn't crash the kernel again
    asm volatile("cli\nhlt");
    while(1);
}

unsigned int do_find_mmap(unsigned int sz, unsigned int upperlimit)
{
    unsigned int back_end = upperlimit;
    while(back_end > 0x100000u)
    {
        if(sz >= 0x7fff000) //need at least 1 empty pgdir
        {
            unsigned int empty_pgdir = check_access_backwards(0x100000u, back_end-4096u, -2) + 4096u;
            back_end = check_access(empty_pgdir, upperlimit, -1);
        }
        else
            back_end = check_access_backwards(0x100000u, back_end-4096u, 0) + 4096u;
        unsigned int front_end = check_access_backwards(0x100000u, back_end-4096u, -1) + 4096u;
        if(back_end - front_end >= sz)
            return back_end - sz;
        back_end = front_end;
    }
    return 0;
}

unsigned int find_mmap(unsigned int addr, unsigned int sz)
{
    if(addr < 0x100000
    || addr + (unsigned long long)sz > 0xc0000000ull
    || check_access(addr, addr+sz, -1) < addr+sz)
        return do_find_mmap(sz, 0x80000000u);
    return addr;
}

unsigned int find_mremap(unsigned int addr, unsigned int sz, unsigned int newsz)
{
    if(newsz < sz)
        return addr;
    unsigned int high = check_access(addr+sz, addr+newsz, -1);
    if(addr + (unsigned long long)newsz <= 0xc0000000ull
    && high == addr+newsz)
        return addr;
    unsigned int low = check_access_backwards(0x100000u, addr, -1) + 4096u;
    if(high - low >= newsz)
        return high - newsz;
    return find_mmap(0, newsz);
}

int check_readable(const void* addr, unsigned int sz)
{
    if((unsigned int)addr < 0x100000u || (unsigned int)addr + (unsigned long long)sz > 0xc0000000ull)
        return -1;
    unsigned int from = (unsigned int)addr;
    unsigned int to = from + sz;
    from &= ~4095u;
    to = (to + 4095u) & ~4095u;
    return (check_access(from, to, 0)==to)?0:-1;
}

int check_writable(void* addr, unsigned int sz)
{
    if((unsigned int)addr < 0x100000u || (unsigned int)addr + (unsigned long long)sz > 0xc0000000ull)
        return -1;
    unsigned int from = (unsigned int)addr;
    unsigned int to = from + sz;
    from &= ~4095u;
    to = (to + 4095u) & ~4095u;
    return (check_access(from, to, 1)==to)?0:-1;
}

int check_string(const char* addr, unsigned int sz_limit)
{
    if((unsigned int)addr < 0x100000u)
        return -1;
    unsigned int from = (unsigned int)addr;
    unsigned long long to_ll = from + (unsigned long long)sz_limit;
    if(to_ll > 0xc0000000ull)
        to_ll = 0xc0000000ull;
    unsigned int to = (unsigned int)to_ll;
    from &= ~4095u;
    to = (to + 4095u) & ~4095u;
    unsigned int length = check_access(from, to, 0)-(unsigned int)addr;
    for(int i = 0; i < length; i++)
        if(!addr[i])
            return 0;
    return -1;
}

void do_cow(void* addr, unsigned int sz)
{
    unsigned int from = (unsigned int)addr;
    unsigned int to = from + sz;
    from &= ~4095u;
    to = (to + 4095u) & ~4095u;
    cow_range(from, to);
}

const char* readlink_proc_self_exe()
{
    return the_exe;
}
