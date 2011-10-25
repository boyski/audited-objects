// gcc -o texecvp -g -W -Wall texecvp.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern char **environ;

int
main(int argc, char *argv[])
{

    argv++, argc--;

    execvp(argv[0], argv);
    perror(argv[0]);
    return 2;
}
