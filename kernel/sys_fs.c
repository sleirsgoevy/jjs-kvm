#include "fs.h"
#include "sys_fs.h"
#include "vmem.h"
#include "paging.h"
#define brk libc_brk
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h> //hope `struct stat` is actually stat64
#undef brk

struct fd fds[1024];
struct file* the_fs;

void reset_sys_fs()
{
    for(int i = 0; i < 1024; i++)
        fds[i] = (struct fd){0, 0};
}

unsigned int sys_read(unsigned int fd, unsigned int buf, unsigned int count, unsigned int _1, unsigned int _2)
{
    if(check_writable((void*)buf, count) < 0)
        return -EFAULT;
    do_cow((void*)buf, count);
    if(fd < 0 || fd >= 1024 || !fds[fd].file)
        return -EBADF;
    return fs_read(fds + fd, (void*)buf, count);
}

unsigned int sys_write(unsigned int fd, unsigned int buf, unsigned int count, unsigned int _1, unsigned int _2)
{
    if(check_readable((void*)buf, count) < 0)
        return -EFAULT;
    if(fd < 0 || fd >= 1024 || !fds[fd].file)
        return -EBADF;
    unsigned int ans = fs_write(fds + fd, (const void*)buf, count);
    if(ans != count)
        return -ENOSYS; //output buffer is full, bailing out
    return ans;
}

unsigned int sys_close(unsigned int fd, unsigned int _1, unsigned int _2, unsigned int _3, unsigned int _4)
{
    if(fd < 0 || fd >= 1024 || !fds[fd].file)
        return -EBADF;
    fds[fd] = (struct fd){0, 0};
    return 0;
}

unsigned int sys_access(unsigned int path_p, unsigned int mode, unsigned int _1, unsigned int _2, unsigned int _3)
{
    if(mode != (mode & (R_OK | W_OK | X_OK)))
        return -ENOSYS;
    if(check_string((const char*)path_p, 4096) < 0)
        return -EFAULT;
    struct fd tmp;
    int ans = fs_open(&tmp, the_fs, (const char*)path_p, 0);
    if(ans < 0)
        return ans;
    if(mode & X_OK)
        return -EACCES;
    if((mode & W_OK) && tmp.file->access == FILE_READONLY)
        return -EACCES;
    return 0;
}

unsigned int sys_fstat64(unsigned int fd, unsigned int struct_p, unsigned int _1, unsigned int _2, unsigned int _3)
{
    if(check_writable((void*)struct_p, sizeof(struct stat)) < 0)
        return -EFAULT;
    if(!fds[fd].file)
        return -EBADF;
    do_cow((void*)struct_p, sizeof(struct stat));
    struct stat* ans = (struct stat*)struct_p;
    *ans = (struct stat){
        .st_mode = 0100444 | (fds[fd].file->access == FILE_READWRITE?0222:0),
        .st_ino = 179,
        .st_dev = 0xfe00,
        .st_nlink = 1,
        .st_uid = 0,
        .st_gid = 0,
        .st_size = fds[fd].file->sz,
        .st_atime = 0,
        .st_ctime = 0,
        .st_mtime = 0
    };
    return 0;
}

unsigned int sys_llseek(unsigned int fd, unsigned int off_high, unsigned int off_low, unsigned int ans_p, unsigned int whence)
{
    if(whence > 2)
        return -EINVAL;
    unsigned long long offset = ((unsigned long long)off_high) << 32 | off_low;
    if(!fds[fd].file)
        return -EBADF;
    if(check_writable((void*)ans_p, 8) < 0)
        return -EFAULT;
    do_cow((void*)ans_p, 8);
    switch(whence)
    {
    case 0:
        break;
    case 1:
        offset += fds[fd].offset;
        break;
    case 2:
        offset += fds[fd].file->sz;
        break;
    }
    if(offset & (1ull << 63)) //negative
        return -EINVAL;
    if(offset >= 0x100000000ull)
        return -ENOSYS;
    *(unsigned long long*)ans_p = fds[fd].offset = offset;
    return 0;
}
