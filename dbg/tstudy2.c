// gcc -o tstudy2 -g -W -Wall tstudy2.c

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

#define PN1	"XXX"
#define PN2	"YYY"
#define PN3	"ZZZ"

int
main(void)
{
    system("mkfile 64 " PN1);

    if (rename(PN1, PN2) != 0)
	die(PN1);

    system("mkfile 64 " PN1);

    if (rename(PN1, PN3) != 0)
	die(PN1);

    return 0;
}
