// gcc -o trename -g -W -Wall trename.c

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

// This should audit the same as a single C (create) op.

int
main(int argc, char *argv[])
{
    char *pn1, *pn2;
    int version = 1;
    int fd = -1;
    char *filedata = "XYZ\n";

    argv++, argc--;

    if (argv[0][0] == '-') {
	version = atoi(argv[0] + 1);
	argv++;
    }

    pn1 = *argv++;
    pn2 = *argv++;

    if (version == 1) {
	printf("create\n");
	if ((fd = open(pn2, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
	    die(pn2);
	(void)write(fd, filedata, strlen(filedata));
	close(fd);
    } else {
	if ((fd = open(pn1, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
	    die(pn1);
	(void)write(fd, filedata, strlen(filedata));
	close(fd);

	if (version == 2) {
	    printf("create, rename\n");
	    if (rename(pn1, pn2) != 0)
		die(pn1);
	} else if (version == 3) {
	    printf("create, link, unlink\n");
	    if (link(pn1, pn2))
		die(pn1);

	    if (unlink(pn1))
		die(pn1);
	}
    }

    return 0;
}
