// gcc -m32 -o topen32 -W -Wall -g -D_REENTRANT -D_LARGEFILE_SOURCE topen.c
// gcc -m64 -o topen64 -W -Wall -g -D_REENTRANT -D_LARGEFILE_SOURCE topen.c

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{
    int fd;
    int rc = 0;
    int i;

    for (i = 1; i < argc; i++) {
	if ((fd = open(argv[i], O_RDONLY)) < 0) {
	    fprintf(stderr, "Error: %s: %s\n", argv[i], strerror(errno));
	    rc++;
	    continue;
	}
	printf("open(%s): i=%d, fd=%d\n", argv[i], i, fd);
	close(fd);
    }

    return rc != 0;
}
