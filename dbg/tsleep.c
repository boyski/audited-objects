// gcc -o tsleep -g -W -Wall tsleep.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int
main(int argc, char *argv[])
{
    int count;
    char buf[16];

    if (argc > 1) {
	count = atoi(argv[1]);
    } else {
	count = 5;
    }

    if (count > 1) {
	sleep(count);
	snprintf(buf, sizeof(buf), "%d", count - 1);
	if (0 && fork()) {
	    wait(NULL);
	} else {
	    execlp(argv[0], argv[0], buf, (char *)0);
	}
    }

    return 0;
}
