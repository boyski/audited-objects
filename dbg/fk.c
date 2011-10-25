#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

extern char **environ;

int
main(int argc, char *argv[])
{
    for (argc = argc, argv++; *argv; argv++) {
	if (!strcmp(*argv, "fork")) {
	    pid_t pid;

	    pid = fork();
	} else if (!strcmp(*argv, "execlp")) {
	    execlp("uname", "uname", (char *)0);
	} else if (!strcmp(*argv, "-execlp")) {
	    execlp("XXXX", "XXXX", (char *)0);
	}
    }
    exit(0);
}
