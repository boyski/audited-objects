// gcc -o simple -g -W -Wall simple.c

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/stat.h>
#endif

int
main(int argc, char *argv[])
{
    int fd;

    for (argc--, argv++; *argv; argc--, argv++) {
	if ((fd = open(*argv, O_RDONLY)) < 0) {
	    perror(*argv);
	}
    }

    return 0;
}
