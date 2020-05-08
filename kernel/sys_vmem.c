#include "sys_vmem.h"
#include "vmem.h"
#include "userspace.h"
#include "sys_fs.h"
#include "paging.h"
#include "debugout.h"
#include "memmap.h"
#include <sys/mman.h>
#include <errno.h>

unsigned int sys_brk(unsigned int x, unsigned int _1, unsigned int _2, unsigned int _3, unsigned int _4)
{
    return brk(x);
}

unsigned int sys_mmap2(unsigned int addr, unsigned int sz, unsigned int prot, unsigned int flags, unsigned int fd)
{
    if(addr & 4095u)
        return -EINVAL;
    sz = (sz + 4095u) & ~4095u;
    unsigned int offset = userspace.gp_regs[GP_REG_EBP]; //6th argument
    if(offset & 0xfff00000u)
        return -ENOSYS;
    offset <<= 12;
    if(flags != (flags & (MAP_SHARED|MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS|MAP_DENYWRITE)))
        return -ENOSYS;
    if((flags & MAP_SHARED) && !(flags & MAP_ANONYMOUS)) //an obscure method of writing to a file
        return -ENOSYS;
    if(sz >= 0xc0000000u)
        return -ENOMEM;
    if(!(flags & MAP_FIXED))
    {
        addr = find_mmap(addr, sz);
        if(!addr)
            return -ENOMEM;
    }
    if(addr < 0x10000 || addr + (unsigned long long)sz > 0xc0000000ull)
        return -EPERM;
    if(!(flags & MAP_ANONYMOUS) && (fd >= 1024 || !fds[fd].file))
        return -EBADF;
    if((flags & MAP_ANONYMOUS))
        memmap_log(addr, addr + sz, (prot & PROT_WRITE)?1:0, "anonymous", 0);
    else
        memmap_log(addr, addr + sz, (prot & PROT_WRITE)?1:0, fds[fd].file->name, offset);
    map_range(addr, addr + sz, (prot & PROT_WRITE)?1:0);
    if(!(prot & PROT_WRITE))
        set_rights(addr, addr + sz, 0);
    volatile char* buf = (volatile char*)addr;
    for(unsigned int i = 0; i < sz; i++) 
        buf[i] = 0;
    if(!(flags & MAP_ANONYMOUS))
    {
        struct fd tmp = fds[fd];
        tmp.offset = offset;
        fs_read(&tmp, (char*)buf, sz);
    }
    return addr;
}

unsigned int sys_mprotect(unsigned int addr, unsigned int sz, unsigned int prot, unsigned int _1, unsigned int _2)
{
    if(addr & 4095u)
        return -EINVAL;
    sz = (sz + 4095u) & ~4095u; // TODO: what if this overflows?
    if(addr < 0x100000 || addr + (unsigned long long)sz > 0xc0000000ull)
        return -EINVAL;
    set_rights(addr, addr + sz, (prot & PROT_WRITE)?1:0);
    return 0;
}

//note: this won't handle file-backed mappings correctly (file-backed mmap is implemented as anonymous mmap + read, see above)
unsigned int sys_mremap(unsigned int addr, unsigned int sz, unsigned int newsz, unsigned int _1, unsigned int _2)
{
    if(addr & 4095u)
        return -EINVAL;
    sz = (sz + 4095u) & ~4095u;
    newsz = (newsz + 4095u) & ~4095u;
    if(addr < 0x100000u
    || addr + (unsigned long long)sz > 0xc0000000ull
    || check_access(addr, addr+sz, 0) < addr+sz)
        return -EFAULT;
    if(sz == newsz)
        return addr;
    if(sz > newsz)
    {
        unmap_range(addr+newsz, addr+sz);
        return addr;
    }
    unsigned int addr2 = find_mremap(addr, sz, newsz);
    if(!addr2)
    {
        unsigned int addr3 = find_mmap(addr, newsz);
        move_pages(addr3, addr, sz);
        unmap_range(addr, addr+sz);
        memmap_log(addr3+sz, addr3+newsz, (check_access(addr3, addr3+4096, 1)==addr3+4096)?1:0, "mremap", 0);
        map_range(addr3+sz, addr3+newsz, (check_access(addr3, addr3+4096, 1)==addr3+4096)?1:0);
        for(unsigned int* p = (unsigned int*)(addr3+sz); p < (unsigned int*)(addr3+newsz); p++)
            *p = 0;
        return addr3;
    }
    else if(addr2 != addr)
    {
        move_pages(addr2, addr, sz);
        unsigned int top = addr2 + sz;
        if(top < addr)
            top = addr;
        unmap_range(top, addr+sz);
    }
    memmap_log(addr2+sz, addr2+newsz, (check_access(addr2, addr2+4096, 1)==addr2+4096)?1:0, "mremap", 0);
    map_range(addr2+sz, addr2+newsz, (check_access(addr2, addr2+4096, 1)==addr2+4096)?1:0);
    for(unsigned int* p = (unsigned int*)(addr2+sz); p < (unsigned int*)(addr2+newsz); p++)
        *p = 0;
    return addr2;
}

unsigned int sys_munmap(unsigned int addr, unsigned int sz, unsigned int _1, unsigned int _2, unsigned int _3)
{
    if(addr < 0x100000 || addr + (unsigned long long)sz > 0xc0000000u)
        return -EPERM;
    sz = (sz+4095u) & ~4095u;
    if(addr < 0x100000 || addr + (unsigned long long)sz > 0xc0000000ull)
        return -EINVAL;
    unmap_range(addr, addr+sz);
    return 0;
}
