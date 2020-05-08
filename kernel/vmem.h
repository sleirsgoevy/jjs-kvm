#pragma once
#include "fs.h"

struct entry
{
    unsigned int eip;
    unsigned int esp;
};

//setup vmem for program execution
int load_executable(struct file* fs, struct fd* f, struct entry* entry, const char* exe, const char** argv, const char** envp);

//setup registers for program execution
void setup_entry_regs(struct entry* entry);

//brk syscall
unsigned int brk(unsigned int x);

//find an address suitable for mmapping (i.e. not intersecting with other mappings) 
unsigned int find_mmap(unsigned int addr, unsigned int sz);
unsigned int find_mremap(unsigned int addr, unsigned int oldsz, unsigned int newsz);

//find an address suitable for mapping, so that the mapping is not higher than `back_end`
unsigned int do_find_mmap(unsigned int sz, unsigned int back_end);

//called only from assembly, so not necessary to have it here, but nevertheless
void kernel_page_fault();

//memory permissions check
int check_readable(const void* addr, unsigned int sz);
int check_writable(void* addr, unsigned int sz);
int check_string(const char* addr, unsigned int sz_limit);

//do CoW (helper)
void do_cow(void* addr, unsigned int sz);

const char* readlink_proc_self_exe();
