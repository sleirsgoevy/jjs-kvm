#pragma once
#include "runtime.h"

syscall_t sys_set_thread_area;

void save_thread_area();
void restore_thread_area();
