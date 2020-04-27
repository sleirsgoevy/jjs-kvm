#include "alloc.h"
#include "paging.h"

unsigned int alloc_addr_low = 0x100000;
unsigned int alloc_addr_high = 0x100000 + (LOW_MEM_SZ);
unsigned int ll_low = 0;
unsigned int ll_high = 0;
unsigned int ml = ML_FULL;
unsigned int ml_orig = ML_FULL;

unsigned int pop_page_low()
{
    unsigned int ans = alloc_addr_low;
    alloc_addr_low += 4096;
    return ans;
}

unsigned int pop_page_high()
{
    unsigned int ans = alloc_addr_high;
    alloc_addr_high += 4096;
    return ans;
}

unsigned int lowmem_alloc_page()
{
    if(ll_low)
    {
        unsigned int ans = ll_low;
        ll_low = *LINEAR(ans);
        volatile unsigned int* p = (volatile unsigned int*)LINEAR(ans);
        for(int i = 0; i < 1024; i++)
            p[i] = 0;
        return ans;
    }
    else
        return pop_page_low();
}

void lowmem_free_page(unsigned int page)
{
    *LINEAR(page) = ll_low;
    ll_low = page;
}

unsigned int highmem_pre_alloc()
{
    if(ml == 0)
        ml = ML_EXCEEDED;
    if(ml == ML_EXCEEDED)
        return alloc_addr_high;
    ml--;
    if(ll_high)
        return ll_high;
    else
        return pop_page_high();
}

void highmem_post_alloc(void* page)
{
    if(ml != ML_EXCEEDED && ll_high)
        ll_high = *(unsigned int*)page;
}

void highmem_free(void* virt, unsigned int phys)
{
    if(ml != ML_EXCEEDED)
    {
        ml++;
        *(unsigned int*)virt = ll_high;
        ll_high = phys;
    }
}

void set_ml(unsigned int new_ml)
{
    if(ml_orig - ml > new_ml)
        ml = ML_EXCEEDED;
    else
        ml += new_ml - ml_orig;
    ml_orig = new_ml;
}
