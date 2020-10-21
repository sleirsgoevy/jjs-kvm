#include "sys_mock.h"
#include "vmem.h"
#define sigprocmask libc_sigprocmask
#include <sys/utsname.h>
#include <errno.h>
#include <signal.h>
#undef sigprocmask

static char* strcpy(char* dst0, const char* src)
{
    char* dst = dst0;
    while(*dst++ = *src++);
    return dst;
}

unsigned int sys_uname(unsigned int ptr, unsigned int _1, unsigned int _2, unsigned int _3, unsigned int _4)
{
    if(check_writable((void*)ptr, sizeof(struct utsname)) < 0)
        return -EFAULT;
    do_cow((void*)ptr, sizeof(struct utsname));
    struct utsname* u = (struct utsname*)ptr;
    strcpy(u->sysname, "Linux");
    strcpy(u->nodename, "localhost");
    strcpy(u->release, "999.99");
    strcpy(u->version, "jjs-kvm");
    strcpy(u->machine, "i686");
    return 0;
}

unsigned int sys_readlink(unsigned int name_p, unsigned int buf_p, unsigned int sz, unsigned int _1, unsigned int _2)
{
    if(check_string((const char*)name_p, 4096) < 0 || check_writable((void*)buf_p, sz) < 0)
        return -EFAULT;
    const char* name = (const char*)name_p;
    for(int i = 0; i < 15; i++)
        if(name[i] != "/proc/self/exe"[i])
            return -ENOSYS;
    if(sz == 0)
        return 0;
    const char* ans = readlink_proc_self_exe();
    char* buf = (char*)buf_p;
    if(ans[0] != '/') // ld-linux requires the path to be absolute
    {
        do_cow(buf, 1);
        *buf++ = '/';
        sz--;
    }
    unsigned int sz_ans;
    for(sz_ans = 0; ans[sz_ans]; sz_ans++);
    sz_ans++;
    if(sz_ans > sz)
        sz_ans = sz;
    do_cow(buf, sz_ans);
    for(int i = 0; i < sz_ans; i++)
        buf[i] = ans[i];
    return sz_ans;
}

static unsigned long long sigprocmask;

void reset_mock()
{
    sigprocmask = 0;
}

unsigned int sys_rt_sigprocmask(unsigned int how, unsigned int mask, unsigned int old, unsigned int sigsetsize, unsigned int _1)
{
    if(sigsetsize != 8)
        return -EINVAL;
    if(how != SIG_BLOCK && how != SIG_UNBLOCK && how != SIG_SETMASK)
        return -EINVAL;
    if(mask && check_readable((const void*)mask, 8) < 0)
        return -EFAULT;
    if(old && check_writable((void*)old, 8) < 0)
        return -EFAULT;
    if(old)
    {
        do_cow((void*)old, 8);
        *(unsigned long long*)old = sigprocmask;
    }
    if(mask)
    {
        const unsigned long long* p = (const unsigned long long*)mask;
        switch(how)
        {
        case SIG_SETMASK:
            sigprocmask = *p;
            break;
        case SIG_BLOCK:
            sigprocmask |= *p;
            break;
        case SIG_UNBLOCK:
            sigprocmask &= ~*p;
            break;
        }
    }
    return 0;
}

unsigned int sys_getpid(unsigned int _1, unsigned int _2, unsigned int _3, unsigned int _4, unsigned int _5)
{
    return 1;
}

unsigned int sys_gettid(unsigned int _1, unsigned int _2, unsigned int _3, unsigned int _4, unsigned int _5)
{
    return 1;
}
