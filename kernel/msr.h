#pragma once

unsigned long long read_msr(unsigned int idx);
void write_msr(unsigned int idx, unsigned long long val);
