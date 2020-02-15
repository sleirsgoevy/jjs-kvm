#include "fs.h"

unsigned int fs_read(struct fd* f, void* buf, unsigned int count)
{
    if(f->offset >= f->file->sz)
        count = 0;
    else if(f->offset + (unsigned long long)count > (unsigned long long)f->file->sz)
        count = f->file->sz - f->offset;
    const char* in = (const char*)f->file->data;
    char* out = (char*)buf;
    for(unsigned int i = 0; i < count; i++)
        out[i] = in[f->offset++];
    return count;
}

unsigned int fs_write(struct fd* f, const void* buf, unsigned int count)
{
    if(f->offset >= f->file->capacity)
        return 0;
    else if(f->offset + (unsigned long long)count > (unsigned long long)f->file->capacity)
        count = f->file->capacity - f->offset;
    const char* in = (const char*)buf;
    char* out = (char*)f->file->data;
    if(f->offset > f->file->sz)
        for(unsigned int i = f->file->sz; i < f->offset; i++)
            out[i] = 0;
    if(f->offset + count > f->file->sz)
        f->file->sz = f->offset + count;
    for(unsigned int i = 0; i < count; i++)
        out[f->offset++] = in[i];
    return count;
}
