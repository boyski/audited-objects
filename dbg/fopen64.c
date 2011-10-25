// gcc -o fopen64 -W -Wall -g -D_LARGEFILE_SOURCE fopen64.c

#include <stdio.h>

int
main(int argc, char *argv[])
{
    FILE *fp;

    (void)argc;

    if ((fp = fopen64(argv[1], "r")) == NULL) {
	return 3;
    } else {
	fclose(fp);
    }
    return 0;
}
