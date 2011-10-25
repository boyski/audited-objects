// gcc -o tallops -g -W -Wall tallops.c

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
    const char *pn1 = "P1";
    const char *pn2 = "P2";
    const char *sl1 = "SL1";
    int fd;

    argv++, argc--;
    pn2 = pn2;

    if ((fd = open(pn1, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) die(pn1);
    close(fd);

    //if (link(pn1, pn2)) die(pn1);

#if 1

    if ((fd = open(pn1, O_RDONLY)) < 0) die(pn1);
    close(fd);

    if (rename(pn1, pn2)) die(pn1);

    if (rename(pn2, pn1)) die(pn2);

    if (unlink(pn1)) die(pn1);

    if ((fd = open(pn1, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) die(pn1);
    write(fd, "XX\n", 3);
    close(fd);

    if (symlink(pn1, sl1)) die(pn1);

    //if (unlink(sl1)) die(sl1);

#endif

    return 0;
}
