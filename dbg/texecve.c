// gcc -o texecve -g -W -Wall texecve.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

static char *envp[1024];

static char *vars[] = {
    "HOME",
    "LD_PRELOAD",
    "LD_PRELOAD_32",
    "PATH",
    NULL,
};

int
main(int argc, char *argv[])
{
    int i;
    char **v, *p, *e;

    argv++, argc--;

    envp[0] = "AX_FOO=2";
    envp[1] = "AB_FOO=1";

    for (i = 2, v = vars; *v; v++) {
	if ((p = getenv(*v))) {
	    e = (char *)calloc(strlen(*v) + 1 + strlen(p) + 1, 1);
	    strcpy(e, *v);
	    strcat(e, "=");
	    strcat(e, p);
	    envp[i++] = e;
	}
    }

    execve(argv[0], argv, envp);
    perror(argv[0]);
    return 0;
}
