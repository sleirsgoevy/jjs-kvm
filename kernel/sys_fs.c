#include "fs.h"
#include "sys_fs.h"
#include "vmem.h"
#include "paging.h"
#include "debugout.h"
#define brk libc_brk
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <asm/stat.h> // struct stat64
#define _FCNTL_H // avoid `Never use ... directly` error
#include <bits/fcntl-linux.h> // avoid `redefinition of struct stat` error
#undef brk

struct fd fds[1024];
static struct fd fds_bak[1024];
struct file* the_fs;

void save_sys_fs()
{
    for(int i = 0; i < 1024; i++)
        fds_bak[i] = fds[i];
}

void restore_sys_fs()
{
    for(int i = 0; i < 1024; i++)
        fds[i] = fds_bak[i];
}

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
    if(!fds[fd].file->data)
        return -ERESTART;
    return fs_read(fds + fd, (void*)buf, count);
}

unsigned int sys_write(unsigned int fd, unsigned int buf, unsigned int count, unsigned int _1, unsigned int _2)
{
    if(check_readable((void*)buf, count) < 0)
        return -EFAULT;
    if(fd < 0 || fd >= 1024 || !fds[fd].file)
        return -EBADF;
    if(!fds[fd].file->data)
        return -ERESTART;
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

static unsigned int do_fstat64(struct fd* fd, unsigned int struct_p)
{
    do_cow((void*)struct_p, sizeof(struct stat64));
    struct stat64* ans = (struct stat64*)struct_p;
    if(!fd->file->data)
        return -ERESTART;
    debug_puts("notice: do_fstat64: inode = ");
    debug_putn(fd->file->inode, 10);
    debug_puts("\n");
    *ans = (struct stat64){
        .st_mode = 0107444 | (fd->file->access == FILE_READWRITE?0222:0),
        .st_ino = fd->file->inode,
        .st_dev = 0xfe00,
        .st_nlink = 1,
        .st_uid = 0,
        .st_gid = 0,
        .st_size = fd->file->sz,
        .st_atime = 0,
        .st_ctime = 0,
        .st_mtime = 0
    };
    return 0;
}

unsigned int sys_fstat64(unsigned int fd, unsigned int struct_p, unsigned int _1, unsigned int _2, unsigned int _3)
{
    if(check_writable((void*)struct_p, sizeof(struct stat64)) < 0)
        return -EFAULT;
    if(!fds[fd].file)
        return -EBADF;
    return do_fstat64(fds+fd, struct_p);
}

unsigned int sys_stat64(unsigned int path, unsigned int struct_p, unsigned int _1, unsigned int _2, unsigned int _3)
{
    const char* path_s = (const char*)path;
    if(check_string(path_s, 4096))
        return -EFAULT;
    if(check_writable((void*)struct_p, sizeof(struct stat64)) < 0)
        return -EFAULT;
    struct fd tmp;
    int status = fs_open(&tmp, the_fs, path_s, 0);
    if(status)
        return status;
    return do_fstat64(&tmp, struct_p);
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
        if(!fds[fd].file->data)
            return -ERESTART;
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

unsigned int sys_open(unsigned int path, unsigned int flags, unsigned int mode, unsigned int _1, unsigned int _2)
{
    const char* path_s = (const char*)path;
    if(check_string(path_s, 4096))
        return -EFAULT;
    if((flags & O_CREAT) && ((mode & 0700) != 0600))
        return -ENOSYS; // POSIX permissions are not implemented
    int fdnum;
    for(fdnum = 0; fdnum < 1024 && fds[fdnum].file; fdnum++);
    if(fdnum == 1024)
        return -ENFILE;
    debug_puts("... open(\"");
    debug_puts((char*)path_s);
    debug_puts("\", 0x");
    debug_putn(flags, 16);
    debug_puts(", 0");
    debug_putn(mode, 8);
    debug_puts(") = ");
    int status = fs_open(fds+fdnum, the_fs, path_s, flags);
    debug_putns(status, 10);
    debug_puts(" ...\n");
    if(status) // failed
    {
        fds[fdnum].file = 0;
        return status;
    }
    return fdnum;
}

unsigned int sys_openat(unsigned int dirfd, unsigned int path, unsigned int flags, unsigned int mode, unsigned int _)
{
    if(dirfd != AT_FDCWD)
        return -ENOSYS;
    return sys_open(path, flags, mode, _, _);
}
