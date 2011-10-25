// gcc -o tabort -g -W -Wall tabort.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

int
main(int argc, char *argv[])
{
    for (argc--, argv++; *argv; argc--, argv++) {
	if (strcmp(*argv, "ABORT")) {
	    (void)system(*argv);
	    sleep(argc);
	} else {
	    char a, *b = NULL;

	    a = *b;
	}
    }

    return 0;
}
