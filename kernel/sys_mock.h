#pragma once
#include "runtime.h"

syscall_t sys_uname;
syscall_t sys_readlink; // /proc/self/exe only, bailout if not
syscall_t sys_rt_sigprocmask;
syscall_t sys_getpid;
syscall_t sys_gettid;
