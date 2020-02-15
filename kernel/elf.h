#pragma once
#include "fs.h"

struct elf_header
{
    char magic[8];
    char pad[8];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned int e_version;
    unsigned int e_entry;
    unsigned int e_phoff;
    unsigned int e_shoff;
    unsigned int e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
} __attribute__((packed));

int validate_elf(struct elf_header*);

struct elf_pheader
{
    unsigned int p_type;
    unsigned int p_offset;
    unsigned int p_vaddr;
    unsigned int p_paddr;
    unsigned int p_filesz;
    unsigned int p_memsz;
    unsigned int p_flags;
    unsigned int p_align;
} __attribute__((packed));

int map_segment(struct fd* f, struct elf_pheader* seg);

unsigned int load_elf(struct fd* f, unsigned int* last_addr, unsigned int* phnum);
