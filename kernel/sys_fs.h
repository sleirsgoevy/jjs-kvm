#pragma once
#include "runtime.h"
#include "fs.h"

extern struct fd fds[];
extern struct file* the_fs;

syscall_t sys_read;
syscall_t sys_write;
syscall_t sys_close;
syscall_t sys_access;
syscall_t sys_fstat64;
syscall_t sys_llseek;

void save_sys_fs();
void restore_sys_fs();
void reset_sys_fs();
