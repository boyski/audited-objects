// gcc -o tfork -g -W -Wall tfork.c

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int
main(int argc, char *argv[])
{
    pid_t pid;

    if (argv[argc-1][0] == 'v') {
	pid = vfork();
    } else {
	pid = fork();
    }

    if (pid) {
	wait(NULL);
    } else {
	execlp("uname", "uname", (char *)0);
    }

    return 0;
}
