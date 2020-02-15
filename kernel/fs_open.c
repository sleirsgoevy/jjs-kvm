#include "fs.h"
#include <fcntl.h> // O_* constants
#include <errno.h> // error constants

static int strcmp(const char* a, const char* b)
{
    int ans;
    while(!(ans = *a - *b) && *a++ && *b++);
    return ans;
}

int fs_open(struct fd* tgt, struct file* fs, const char* name, int flags)
{
    if(flags != (flags & (O_RDONLY|O_WRONLY|O_RDWR|O_CREAT|O_CLOEXEC|O_TRUNC)))
        return -ENOSYS;
    int is_write = (flags & O_RDWR || flags & O_RDONLY);
    if(!is_write && flags & (O_CREAT|O_TRUNC))
        return -EINVAL;
    while(name[0] == '.' && name[1] == '/')
        name += 2;
    while(fs && strcmp(fs->name, name))
        fs = fs->next;
    if(!fs)
        return -ENOENT;
    if(is_write && fs->access == FILE_READONLY)
        return -EACCES;
    if(flags & O_TRUNC)
        fs->sz = 0;
    tgt->file = fs;
    tgt->offset = 0;
    return 0;
}
