// gcc -g -W -Wall -o multilink multilink.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    printf("Run as '%s'\n", argv[0]);
    _exit(0);
}
