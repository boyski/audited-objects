// gcc -o texit -g -W -Wall texit.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

int
main(int argc, char *argv[])
{
    int rc = 0;

    if (argc > 2) {
	rc = atoi(argv[2]);
    }

    if (argc < 2 || argv[1][0] == 'r' || !argv[1][0]) {
	fprintf(stderr, "Ending with return(%d)\n", rc);
	return rc;
    } else if (argv[1][0] == 'e') {
	fprintf(stderr, "Exiting with exit(%d)\n", rc);
	exit(rc);
    } else if (argv[1][0] == '_') {
	fprintf(stderr, "Exiting with _exit(%d)\n", rc);
	_exit(rc);
#if defined(linux)
    } else if (argv[1][0] == 'g') {
	// Note: exit_group is a linux function which was added
	// with kernel 2.5.xx distributions.
	fprintf(stderr, "Exiting with exit_group(%d)\n", rc);
	exit_group(rc);
#endif
#if 1
    } else if (argv[1][0] == 'E') {
	// Note: _Exit is a C99 function which may need to be
	// commented out on older systems such as Solaris 9.
	fprintf(stderr, "Exiting with _Exit(%d)\n", rc);
	_Exit(rc);
#endif
    } else if (argv[1][0] == 'a') {
	fprintf(stderr, "Aborting\n");
	abort();
    }

    return -1;
}
