#pragma once

void save_runtime();
void restore_runtime();
void reset_runtime();
int main_loop(unsigned long long tl);

typedef unsigned int syscall_t(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);

enum{BAILOUT = 0x80000000, ML = 0x80000001, TL = 0x80000002, RESTART = 0x80000003};
