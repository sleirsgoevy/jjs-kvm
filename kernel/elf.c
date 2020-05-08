#include "elf.h"
#include "paging.h"
#include "debugout.h"
#include "vmem.h"
#include "memmap.h"

static void* memset(void* s, int c, unsigned int n)
{
    char* buf = (char*)s;
    for(unsigned i = 0; i < n; i++)
        buf[i] = c;
    return s;
}

int validate_elf(struct elf_header* hdr)
{
    for(unsigned int i = 0; i < 7; i++)
        if(hdr->magic[i] != "\x7f""ELF\x01\x01\x01"[i])
            return -1;
    if(hdr->magic[7] != 0 && hdr->magic[7] != 3)
        return -1;
    if((hdr->e_type != 2 && hdr->e_type != 3) || hdr->e_machine != 3 || hdr->e_version != 1 || hdr->e_ehsize != 52 || hdr->e_phentsize != 32 || hdr->e_phnum == 0)
        return -1;
    return 0;
}

int map_segment(struct fd* f, struct elf_pheader* hdr)
{
    if(hdr->p_type != 1)
        return 0; //not for load
    if(hdr->p_vaddr < 0x100000u)
        return -1;
    if(hdr->p_vaddr + (unsigned long long)hdr->p_memsz > 0xc0000000ull)
        return -1;
    unsigned int low = hdr->p_vaddr;
    unsigned int high = low + hdr->p_memsz;
    debug_puts("map_segment: low=0x");
    debug_putn(low, 16);
    debug_puts(", high=0x");
    debug_putn(high, 16);
    debug_puts("\n");
    unsigned int lowp = low, highp = high;
    lowp &= ~4095u;
    highp = ((highp - 1) | 4095u) + 1;
    memmap_log(lowp, highp, (hdr->p_flags&2?1:0), f->file->name, hdr->p_offset);
    map_range(lowp, highp, (hdr->p_flags&2?1:0));
    char* dst = (char*)low;
    memset(dst, 0, hdr->p_memsz);
    f->offset = hdr->p_offset;
    fs_read(f, dst, hdr->p_filesz);
    return 0;
}

unsigned int load_elf(struct fd* f, unsigned int* last_addr, unsigned int* phdr_addr, unsigned int* phnum, unsigned int* at_base, char* interp, unsigned int interp_sz)
{
    if(interp)
        *interp = 0;
    *last_addr = 0x8000000;
    f->offset = 0;
    struct elf_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    fs_read(f, &hdr, sizeof(hdr));
    if(validate_elf(&hdr) < 0)
        return 0;
    unsigned int offset = hdr.e_phoff;
    unsigned int minoff = 0xffffffffu;
    unsigned long long sz = 0;
    for(unsigned int i = 0; i < hdr.e_phnum; i++)
    {
        struct elf_pheader phdr;
        memset(&phdr, 0, sizeof(phdr));
        f->offset = offset;
        fs_read(f, &phdr, sizeof(phdr));
        if(phdr.p_vaddr+(unsigned long long)phdr.p_memsz > sz)
            sz = phdr.p_vaddr+(unsigned long long)phdr.p_memsz;
        if(phdr.p_vaddr < minoff)
            minoff = phdr.p_vaddr;
        offset += 32;
    }
    offset = hdr.e_phoff;
    if(sz >= 0x100000000u)
        return 0;
    minoff &= ~4095u;
    unsigned int vbase = 0;
    if(hdr.e_type == 3) // ET_DYN
    {
        vbase = do_find_mmap((sz - minoff + 4095u) & ~4095u, 0x80000000u) + minoff;
        debug_puts("load_elf: PIE, vbase=");
        debug_putn(vbase, 16);
        debug_puts("\n");
    }
    *last_addr = vbase + sz;
    int have_phdr = 0;
    for(unsigned int i = 0; i < hdr.e_phnum; i++)
    {
        struct elf_pheader phdr;
        memset(&phdr, 0, sizeof(phdr));
        f->offset = offset;
        fs_read(f, &phdr, sizeof(phdr));
        phdr.p_vaddr += vbase;
        if(phdr.p_type == 6) // PT_PHDR
        {
            if(phnum)
            {
                *phdr_addr = phdr.p_vaddr;
                *phnum = hdr.e_phnum;
                have_phdr = 1;
            }
            phdr.p_type = 1; // PT_LOAD
        }
        if(interp && phdr.p_type == 3) // PT_INTERP
        {
            if(phdr.p_filesz >= interp_sz) // trailing zero
                return 0;
            f->offset = phdr.p_offset;
            memset(interp, 0, sizeof(interp));
            fs_read(f, interp, phdr.p_filesz);
            phdr.p_type = 1; // PT_LOAD
        }
        if(phdr.p_type == 2) // PT_DYNAMIC
        // dyld needs this for proper operation
            phdr.p_type = 1; // PT_LOAD
        if(map_segment(f, &phdr) < 0)
            return 0;
        offset += 32;
    }
    if(phnum && !have_phdr)
    {
        unsigned int phdr_sz = hdr.e_phnum << 5;
        unsigned int addr = do_find_mmap((phdr_sz+4095u)&~4095u, 0x80000000u);
        memmap_log(addr, addr+((phdr_sz+4095u)&~4095u), 0, "AT_PHDR", 0);
        map_range(addr, addr+((phdr_sz+4095u)&~4095u), 0);
        memset((void*)addr, 0, (phdr_sz+4095u)&~4095u);
        f->offset = hdr.e_phoff;
        fs_read(f, (void*)addr, phdr_sz);
        *phdr_addr = addr;
        *phnum = hdr.e_phnum;
    }
    if(at_base)
        *at_base = vbase;
    return hdr.e_entry + vbase;
}
