#include "done.h"

void done(int status)
{
    const char* s = (status==0?"DONE\n":"ERROR\n");
    //not using debugout, so it works with ndebug
    for(int i = 0; s[i]; i++)
        asm volatile("out %%al, $0xe9"::"a"(s[i]));
}
