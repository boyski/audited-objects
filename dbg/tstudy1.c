// gcc -o tstudy1 -g -W -Wall tstudy1.c

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
main(void)
{
    int fd;

    if ((fd = creat("YYY", 0666)) == -1) {
	die("YYY");
    } else {
	write(fd, "AA\n", 3);
	close(fd);
    }

    system("mv YYY XXX");

    system("mv XXX ZZZ");

    if ((fd = creat("YYY", 0666)) == -1) {
	die("YYY");
    } else {
	write(fd, "BB\n", 3);
	close(fd);
    }

    return 0;
}
