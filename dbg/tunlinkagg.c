// gcc -o tunlinkagg -g -W -Wall tunlinkagg.c

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

// This tests the handling of write/remove sequences (for temp files etc).

int
main(int argc, char *argv[])
{
    char *pn1;
    int fd = -1;
    int i;

    pn1 = argv[1];

    for (i = 2; i < argc; i++) {
	switch (argv[i][0]) {
	    case '0': unlink(pn1); break;
	    case '1':
		if ((fd = creat(pn1, 0666)) == -1) {
		    die(pn1);
		}
		write(fd, argv[i], strlen(argv[i]));
		write(fd, "\n", 1);
		close(fd);
		break;
	}

    }

    return 0;
}
