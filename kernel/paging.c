#include "paging.h"
#include "alloc.h"
#include "debugout.h"

void* memcpy(void* dst, const void* src, unsigned int cnt);
void* memset(void* s, int c, unsigned int n);

static unsigned int cr3;
static unsigned int orig_cr3;

static void set_cr3(unsigned int x)
{
    cr3 = x;
    asm volatile("mov %0, %%cr3"::"r"(x));
}

void init_paging()
{
    unsigned int pgdir_addr = pop_page_low();
    unsigned int* pgdir = (unsigned int*)pgdir_addr;
    memset(pgdir, 0, 4096);
    /*for(int i = 1; i < 1024; i++)
        pgdir[i] = 0;*/
    unsigned int pgtbl_addr = pop_page_low();
    unsigned int* pgtbl = (unsigned int*)pgtbl_addr;
    pgtbl[0] = 0;
    for(int i = 1; i < 256; i++)
        pgtbl[i] = (i << 12) | 0x103;
    memset(pgtbl + 256, 0, 3072);
    /*for(int i = 256; i < 1024; i++)
        pgtbl[i] = 0;*/
    pgdir[0] = pgtbl_addr | 7; // spans whole 4mb, so upper part can be used for userspace allocations
    int cntr = 0;
    for(int i = 768; i < 1024; i++)
    {
        pgtbl_addr = pop_page_low();
        pgtbl = (unsigned int*)pgtbl_addr;
        for(int j = 0; j < 1024; j++)
            pgtbl[j] = (cntr++ << 12) | 0x103;
        pgdir[i] = pgtbl_addr | 3;
    }
    orig_cr3 = pgdir_addr;
    set_cr3(orig_cr3);
    asm volatile("mov %cr0, %eax\nor $0x80000000, %eax\nmov %eax, %cr0"); //enable paging
}

static void cow_prepare_tbl(unsigned int* tbl, int offset)
{
    for(int i = offset; i < 1024; i++)
        if(tbl[i])
        {
            tbl[i] |= (tbl[i] & 2) << 8 | 1024u;
            tbl[i] &= ~2;
        }
}

void cow_prepare()
{
    unsigned int* pgdir = LINEAR(cr3);
    cow_prepare_tbl(LINEAR(pgdir[0] & ~4095u), 256);
    for(int i = 1; i < 768; i++)
        if(pgdir[i])
        {
            cow_prepare_tbl(LINEAR(pgdir[i] & ~4095u), 0);
            pgdir[i] |= 1024u;
        }
}

static unsigned int bak1, bak2, bak3, bak4, bak5, bak6;

static void alloc_mark()
{
    bak1 = alloc_addr_low;
    bak2 = alloc_addr_high;
    bak3 = ll_low;
    bak4 = ll_high;
    bak5 = ml;
    bak6 = ml_orig;
    ll_low = ll_high = 0; //prevent free of currently allocated pages
}

static void alloc_reset()
{
    alloc_addr_low = bak1;
    alloc_addr_high = bak2;
    ll_low = bak3;
    ll_high = bak4;
    ml = bak5;
    ml_orig = bak6;
}

void clone_paging()
{
    alloc_mark();
    unsigned int new_pgdir = pop_page_low();
    for(int i = 0; i < 1024; i++)
        LINEAR(new_pgdir)[i] = LINEAR(cr3)[i];
    set_cr3(new_pgdir);
}

static void invlpg(unsigned int addr)
{
    asm volatile("invlpg (%0)"::"r"(addr));
}

static unsigned int get_cow(unsigned int pgdir)
{
    static unsigned int page[1024];
    unsigned int* pg;
    if(pgdir != 0)
    {
        if(!(pgdir & 1024u))
            return pgdir;
        pg = LINEAR(pgdir & ~4095u);
        memcpy(page, pg, 4096);
        /*for(int i = 0; i < 1024; i++)
            page[i] = pg[i];*/
    }
    else
        memset(page, 0, 4096);
        /*for(int i = 0; i < 1024; i++)
            page[i] = 0;*/
    pgdir = lowmem_alloc_page() | 7;
    pg = LINEAR(pgdir & ~4095u);
    memcpy(pg, page, 4096);
    /*for(int i = 0; i < 1024; i++)
        pg[i] = page[i];*/
    return pgdir;
}

static unsigned int map_pgdir(unsigned int* pgdir, unsigned int base, unsigned int low, unsigned int high, int rw)
{
    if(low < base)
        low = base;
    if(high > base + 1024)
        high = base + 1024;
    low -= base;
    high -= base;
    for(unsigned int i = low; i < high; i++)
    {
        int dirty = 0;
        int new = 0;
        if(!pgdir[i])
        {
            new = 1;
            dirty = 1;
            pgdir[i] = highmem_pre_alloc() | (rw?2:0) | 5;
        }
        else if(pgdir[i] & 1024u)
        {
            static unsigned int page[1024];
            unsigned int* pg = (unsigned int*)(pgdir[i] & ~4095u);
            memcpy(page, pg, 4096);
            /*for(int i = 0; i < 1024; i++)
                page[i] = pg[i];*/
            pgdir[i] = highmem_pre_alloc() | (pgdir[i] & 0x9ffu) | (pgdir[i] & 512u) >> 8;
            invlpg((base+i)<<12);
            highmem_post_alloc((void*)((base+i)<<12));
            memcpy(pg, page, 4096);
            /*for(int i = 0; i < 1024; i++)
                pg[i] = page[i];*/
        }
        if(rw && !(pgdir[i] & 2))
        {
            dirty = 1;
            pgdir[i] |= 2;
        }
        if(dirty)
            invlpg((base+i)<<12);
        if(new)
            highmem_post_alloc((void*)((base+i)<<12));
    }
    return high - low;
}

void map_range(unsigned int from, unsigned int to, int rw)
{
    from >>= 12;
    to >>= 12;
    for(unsigned int pgdir = (from >> 10); pgdir <= ((to-1) >> 10); pgdir++)
    {
        //SUBOPTIMAL, needs rewriting. unnecessary page allocation/deallocation possible
        unsigned int old_pgdir = LINEAR(cr3)[pgdir];
        LINEAR(cr3)[pgdir] = get_cow(old_pgdir);
        if(!map_pgdir(LINEAR(LINEAR(cr3)[pgdir] & ~4095u), pgdir << 10, from, to, rw) && old_pgdir != LINEAR(cr3)[pgdir])
        {
            lowmem_free_page(LINEAR(cr3)[pgdir] & ~4095u);
            LINEAR(cr3)[pgdir] = old_pgdir;
        }
    }
}

static int unmap_pgdir(unsigned int* pgdir, unsigned int base, unsigned int from, unsigned int to)
{
    if(from < base)
        from = base;
    if(to > base + 1024)
        to = base + 1024;
    from -= base;
    to -= base;
    for(unsigned int i = from; i < to; i++)
    {
        if(pgdir[i])
        {
            if(!(pgdir[i] & 1024u))
                highmem_free((void*)((base+i)<<12), pgdir[i] & ~4095u);
            else
                highmem_increment_ml();
            pgdir[i] = 0;
            invlpg((base+i)<<12);
        }
    }
    for(int i = 0; i < from; i++)
        if(pgdir[i])
            return 0;
    for(int i = to; i < 1024; i++)
        if(pgdir[i])
            return 0;
    return 1;
}

void unmap_range(unsigned int from, unsigned int to)
{
    from >>= 12;
    to >>= 12;
    for(unsigned int i = (from >> 10); i <= ((to-1) >> 10); i++)
        if(LINEAR(cr3)[i] && unmap_pgdir(LINEAR((LINEAR(cr3)[i] = get_cow(LINEAR(cr3)[i])) & ~4095u), i << 10, from, to))
        {
            lowmem_free_page(LINEAR(cr3)[i] & ~4095u);
            LINEAR(cr3)[i] = 0;
        }
}

static void set_rights_pgdir(unsigned int* pgdir, unsigned int base, unsigned int from, unsigned int to, int w)
{
    if(from < base)
        from = base;
    if(to > base + 1024)
        to = base + 1024;
    from -= base;
    to -= base;
    for(unsigned int i = from; i < to; i++)
        if(pgdir[i])
        {
            if(w)
                pgdir[i] |= ((pgdir[i]&1024u)?512u:2u);
            else
                pgdir[i] &= ~514u;
        }
}

void set_rights(unsigned int from, unsigned int to, int w)
{
    from >>= 12;
    to >>= 12;
    for(unsigned int i = (from >> 10); i <= ((to-1) >> 10); i++)
        if(LINEAR(cr3)[i])
            set_rights_pgdir(LINEAR((LINEAR(cr3)[i] = get_cow(LINEAR(cr3)[i])) & ~4095u), i << 10, from, to, w);
}

static unsigned int cow_pgdir(unsigned int* pgdir, unsigned int base, unsigned int from, unsigned int to)
{
    if(from < base)
        from = base;
    if(to > base + 1024)
        to = base + 1024;
    from -= base;
    to -= base;
    unsigned int cowed = 0;
    for(unsigned int i = from; i < to; i++)
    {
        if(pgdir[i] & 512) // CoW bit
        {
            unsigned int* pg = (unsigned int*)((base+i)<<12);
            static unsigned int page[1024];
            for(int j = 0; j < 1024; j++)
                page[j] = pg[j];
            highmem_increment_ml();
            unsigned int new_page = highmem_pre_alloc();
            pgdir[i] = new_page | 7;
            invlpg((base+i)<<12);
            highmem_post_alloc((void*)((base+i)<<12));
            for(int j = 0; j < 1024; j++)
                pg[j] = page[j];
            cowed++;
        }
    }
    return cowed;
}

unsigned int cow_range(unsigned int from, unsigned int to)
{
    from >>= 12;
    to >>= 12;
    unsigned int cowed = 0;
    for(unsigned int i = (from >> 10); i <= ((to-1) >> 10); i++)
        if(LINEAR(cr3)[i])
            cowed += cow_pgdir(LINEAR((LINEAR(cr3)[i] = get_cow(LINEAR(cr3)[i])) & ~4095u), i << 10, from, to);
    return cowed;
}

static unsigned int check_access_pgdir(unsigned int* pgdir, unsigned int base, unsigned int from, unsigned int to, int perms)
{
    if(from < base)
        from = base;
    if(to > base + 1024)
        to = base + 1024;
    from -= base;
    to -= base;
    for(unsigned int i = from; i < to; i++)
    {
        int ok;
        switch(perms)
        {
        case -1:
            ok = (pgdir[i] == 0);
            break;
        case 1:
            ok = ((pgdir[i] & 514u) != 0); // writable or CoW-writable, either is ok
            break;
        default:
            ok = (pgdir[i] != 0);
        }
        if(!ok)
            return base + i;
    }
    return base + to;
}

unsigned int check_access(unsigned int from, unsigned int to, int perms)
{
    from >>= 12;
    unsigned int from0 = from << 12;
    to >>= 12;
    for(unsigned int i = (from >> 10); i <= ((to-1) >> 10); i++)
    {
        if(!LINEAR(cr3)[i])
        {
            if(perms == -1)
                continue;
            unsigned ans = i << 22;
            return ans<from0?from0:ans;
        }
        if(perms != -2)
        {
            unsigned int ans = check_access_pgdir(LINEAR(LINEAR(cr3)[i] & ~4095u), i << 10, from, to, perms);
            if(ans != (i+1)<<10)
                return ans << 12;
        }
    }
    return to << 12;
}

static unsigned int check_access_pgdir_backwards(unsigned int* pgdir, unsigned int base, unsigned int from, unsigned int to, int perms)
{
    if(from > base + 1023)
        from = base + 1023;
    if(to < base)
        to = base;
    from -= base;
    to -= base;
    for(unsigned int i = from; i >= to; i--)
    {
        int ok;
        switch(perms)
        {
        case -1:
            ok = (pgdir[i] == 0);
            break;
        case 1:
            ok = ((pgdir[i] & 514u) != 0); // writable or CoW-writable, either is ok
            break;
        default:
            ok = (pgdir[i] != 0);
        }
        if(!ok)
            return base + i;
    }
    return base + to - 1;
}

unsigned int check_access_backwards(unsigned int to, unsigned int from, int perms)
{
    from >>= 12;
    unsigned int from0 = from << 12;
    to >>= 12;
    for(unsigned int i = ((from-1) >> 10); i >= (to >> 10); i--)
    {
        if(!LINEAR(cr3)[i])
        {
            if(perms == -1)
                continue;
            unsigned int ans = i << 22 | 0x3ff000u;
            return ans>=from0?from0-4096u:ans;
        }
        if(perms != -2)
        {
            unsigned int ans = check_access_pgdir_backwards(LINEAR(LINEAR(cr3)[i] & ~4095u), i << 10, from, to, perms);
            if(ans != (i<<10)-1)
                return ans << 12;
        }
    }
    return (to-1) << 12;
}

void move_pages(unsigned int to, unsigned int from, unsigned int sz)
{
    if(to == from)
        return;
    to >>= 12;
    from >>= 12;
    unsigned int npages = sz >> 12;
    unsigned int i1a, i1b, i2a, i2b, step;
    if(to < from)
    {
        i1b = to;
        i2b = from;
        step = 1;
    }
    else
    {
        i1b = to + npages - 1;
        i2b = from + npages - 1;
        step = -1;
    }
    i1a = i1b >> 10;
    i1b &= 1023u;
    i2a = i2b >> 10;
    i2b &= 1023u;
    unsigned int* pd = LINEAR(cr3);
    unsigned int* pt1 = LINEAR((pd[i1a] = get_cow(pd[i1a])) & ~4095u);
    unsigned int* pt2 = LINEAR((pd[i2a] = get_cow(pd[i2a])) & ~4095u);
    for(unsigned int i = 0; i < npages; i++)
    {
        pt1[i1b] = pt2[i2b];
        pt2[i2b] |= 1024u; //for unmap_range to not deallocate
        invlpg(i1a<<22|i1b<<12);
        i1b += step;
        i2b += step;
        if(i1b == ~0u)
        {
            i1b = 1023;
            i1a--;
            pt1 = LINEAR((pd[i1a] = get_cow(pd[i1a])) & ~4095u);
        }
        if(i1b == 1024)
        {
            i1b = 0;
            i1a++;
            pt1 = LINEAR((pd[i1a] = get_cow(pd[i1a])) & ~4095u);
        }
        if(i2b == ~0u)
        {
            i2b = 1023;
            i2a--;
            pt2 = LINEAR((pd[i2a] = get_cow(pd[i2a])) & ~4095u);
        }
        if(i2b == 1024)
        {
            i2b = 0;
            i2a++;
            pt2 = LINEAR((pd[i2a] = get_cow(pd[i2a])) & ~4095u);
        }
    }
}

void pop_paging()
{
    set_cr3(orig_cr3);
    alloc_reset();
}

#ifndef NDEBUG
void debug_print_pgtblent(unsigned int addr)
{
    int idx1 = addr >> 22;
    int idx2 = (addr >> 12) & 1023;
    debug_puts("cr3 = ");
    debug_putn(cr3, 16);
    debug_putc('\n');
    unsigned int* pgdir = LINEAR(cr3);
    debug_puts("pgdir[");
    debug_putn(idx1, 10);
    debug_puts("] = ");
    debug_putn(pgdir[idx1], 16);
    debug_putc('\n');
    debug_puts("pgtbl[");
    debug_putn(idx2, 10);
    debug_puts("] = ");
    if(!pgdir[idx1])
        debug_puts("nonexistent (pgdir empty)");
    else
    {
        unsigned int* pgtbl = LINEAR(pgdir[idx1] & ~4095u);
        debug_putn(pgtbl[idx2], 16);
    }
    debug_putc('\n');
}
#endif
