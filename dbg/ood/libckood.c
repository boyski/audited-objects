// Sun: gcc -g -W -Wall -fpic -shared -static-libgcc -o libckood.so libckood.c
// Linux: gcc -g -W -Wall -fpic -shared -o libckood.so libckood.c
// OSX: cc -g -W -Wall -dynamiclib -flat_namespace -o libckood.so libckood.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
oodfunc(const char *tgt)
{
    printf("Using cksum technique for target '%s'\n", tgt);
    return 0;
}
