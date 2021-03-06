#pragma once
#include "fs.h"

struct test
{
    unsigned long long tl; // in microseconds
    unsigned int ml; // in pages (4096B each)
    const char* input_txt;
    const char* output_txt;
    struct file* fs;
    volatile int outcome; // guest to host communication, should be left zero
};

struct bundle
{
    unsigned int size;
    unsigned int ntests;
    const char* executable;
    struct test tests[];
};

extern struct bundle* bundle;

void locate_bundle();
