// gcc -o tsymlink -g -W -Wall tsymlink.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static void
die(const char *str)
{
    perror(str);
    exit(2);
}

int
main(int argc, char *argv[])
{
    assert(argc == 3);

    if (chdir("subdir"))
	die("subdir");

    if (symlink(argv[1], argv[2]))
	die(argv[2]);

    return 0;
}
