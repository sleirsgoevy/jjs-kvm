#include <stdio.h>
#include <stdlib.h>

int main()
{
    free(realloc(malloc(100000000), 200000000));
    printf("Hello, world!\n");
    return 0;
}
