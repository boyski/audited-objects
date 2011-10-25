// gcc -o tlink -g -W -Wall tlink.c

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

#if 1
    {
	int fd;

	if ((fd = creat(argv[1], 0666)) == -1)
	    die(argv[1]);
	write(fd, "stuff\n", 6);
	close(fd);
    }
#endif

    if (link(argv[1], argv[2]))
	die(argv[2]);

    return 0;
}
