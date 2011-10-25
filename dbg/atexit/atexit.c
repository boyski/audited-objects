/*
 * Build: gcc -g -W -Wall -o atexit atexit.c
*/

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

void bye(void) {
    printf("That was all, folks\n");
}

int
main(void)
{
    atexit(bye);

    return 0;
}
