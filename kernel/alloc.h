#pragma once

//allocator state (to be saved&restored)
extern unsigned int alloc_addr_low;
extern unsigned int alloc_addr_high;
extern unsigned int ll_low;
extern unsigned int ll_high;
extern unsigned int ml;
extern unsigned int ml_orig;
#define ML_FULL 0xfffffffeu
#define ML_EXCEEDED 0xffffffffu

//move-forward allocator (to be used in init_paging())
#define LOW_MEM_SZ (15*1024*1024) // 1MB to 16MB
unsigned int pop_page_low();
unsigned int pop_page_high();

//lowmem alloc
unsigned int lowmem_alloc_page();
void lowmem_free_page(unsigned int page);

/*highmem alloc (complicated)*/

//start allocation. returns a physical address inside the new page (possibly misaligned)
unsigned int highmem_pre_alloc();

//finish allocation. `page` must be a virtual address
void highmem_post_alloc(void* page);

//deallocate highmem page
void highmem_free(void* virt, unsigned int phys);

void set_ml(unsigned int new_ml);
