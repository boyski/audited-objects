/*
 * Build: gcc -m32 -o hello32 -g hello.c
 *    or: gcc -m64 -o hello64 -g hello.c
 */

#include <stdio.h>

#ifdef __LP64__
int bitness = 64;
#else
int bitness = 32;
#endif

int
main(void)
{
    printf("Hello %d-bit world!\n", bitness);
    return 0;
}
