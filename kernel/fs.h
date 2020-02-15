#pragma once

struct file
{
    const char* name;
    enum
    {
        FILE_READONLY,
        FILE_READWRITE
    } access;
    void* data;
    unsigned int sz;
    unsigned int capacity;
    struct file* next;
};

struct fd
{
    struct file* file;
    unsigned int offset;
};

int fs_open(struct fd* tgt, struct file* fs, const char* name, int flags);
unsigned int fs_read(struct fd* f, void* buf, unsigned int cnt);
unsigned int fs_write(struct fd* f, const void* buf, unsigned int cnt);
