#pragma once

void reset_runtime();
int main_loop(unsigned long long tl);

typedef unsigned int syscall_t(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
