#include "elf.h"
#include "paging.h"
#include "debugout.h"

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
    if(hdr->e_type != 2 || hdr->e_machine != 3 || hdr->e_version != 1 || hdr->e_ehsize != 52 || hdr->e_phentsize != 32 || hdr->e_phnum == 0)
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
    map_range(lowp, highp, (hdr->p_flags&2?1:0));
    char* dst = (char*)low;
    memset(dst, 0, hdr->p_memsz);
    f->offset = hdr->p_offset;
    fs_read(f, dst, hdr->p_filesz);
    return 0;
}

unsigned int load_elf(struct fd* f, unsigned int* last_addr, unsigned int* phnum)
{
    *last_addr = 0x8000000;
    f->offset = 0;
    struct elf_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    fs_read(f, &hdr, sizeof(hdr));
    if(validate_elf(&hdr) < 0)
        return 0;
    unsigned int offset = hdr.e_phoff;
    for(unsigned int i = 0; i < hdr.e_phnum; i++)
    {
        struct elf_pheader phdr;
        memset(&phdr, 0, sizeof(phdr));
        f->offset = offset;
        fs_read(f, &phdr, sizeof(phdr));
        if(map_segment(f, &phdr) < 0)
            return 0;
        else
        {
            unsigned int tail = phdr.p_vaddr + phdr.p_memsz;
            if(tail > *last_addr)
                *last_addr = tail;
        }
        offset += 32;
    }
    struct elf_pheader phdr_itself = {
        .p_type = 1,
        .p_vaddr = *last_addr,
        .p_offset = hdr.e_phoff,
        .p_filesz = 32 * hdr.e_phnum,
        .p_memsz = 32 * hdr.e_phnum,
        .p_flags = 0
    };
    if(map_segment(f, &phdr_itself) < 0)
        return 0;
    *phnum = hdr.e_phnum;
    return hdr.e_entry;
}
