#ifdef NDEBUG
#define memmap_log(...)
#define memmap_print()
#else
void memmap_log(unsigned int start, unsigned int end, int is_rw, const char* file, unsigned int offset);
void memmap_print();
#endif
