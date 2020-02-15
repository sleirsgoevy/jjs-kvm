#pragma once

//linear mapping of the physical memory. only covers low 1G
#define LINEAR(x) ((unsigned int*)((x) + 0xc0000000u))

void init_paging();
void cow_prepare();
void clone_paging();

//note: for these functions, lower 12 bits in `from` and `to` are discarded. They should normally be set to 0.
void map_range(unsigned int from, unsigned int to, int rw);
void unmap_range(unsigned int from, unsigned int to);
void set_rights(unsigned int from, unsigned int to, int rw);
unsigned int cow_range(unsigned int from, unsigned int to); //returns number of successfully CoW-ed pages

//returns the address of the first page that doesn't satisfy the requirements
/* perms:
-2 search for empty pgdir (returns the first page in that; must be none to pass)
-1 search for present pages (must be all inaccessible to pass)
0 search for non-present pages (must be all readable to pass)
1 search for non-writable or non-present pages (must be all present and writable to pass)
*/
unsigned int check_access(unsigned int from, unsigned int to, int perms);
//used by the memory allocator to find a suitable address. same as above but goes backwards
unsigned int check_access_backwards(unsigned int to, unsigned int from, int perms);

//used by mremap
void move_pages(unsigned int to, unsigned int from, unsigned int sz);

void pop_paging();

#ifdef NDEBUG
static inline void debug_print_pgtblent(unsigned int addr){}
#else
void debug_print_pgtblent(unsigned int addr);
#endif
