// gcc $(getconf LFS_CFLAGS) -o largefile largefile.c

#include <stdio.h>
#include <stdlib.h>

#define L40 "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
#define LINE L40 L40

int
main(int argc, char *argv[])
{
    FILE *fp;
    char *lf = "LARGE.X";
    int power, delta, len;
    off64_t size;

    power = atoi(argv[1]);
    delta = atoi(argv[2]);
    size = (1 << power) + delta;

    fp = fopen(lf, "w");
    while (size > 0) {
	len = (size > 80) ? 80 : size;
	size -= len;
	fwrite(LINE, len - 1, 1, fp);
	fputc('\n', fp);
    }
    fclose(fp);

    return 0;
}
