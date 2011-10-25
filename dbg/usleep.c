// gcc -o usleep -W -Wall -g usleep.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

int
main(int argc, char *argv[])
{
    useconds_t delay;

    delay = argc > 1 ? atoi(argv[1]) : 0;

    usleep(delay);

    return 0;
}
