#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern char **environ;

int
main(void)
{
    char **env;

    printf("environ=0x%lx\n", (long)environ);
    putenv(strdup("FOO=bar"));
    printf("environ=0x%lx\n", (long)environ);
    putenv(strdup("BAR=baz"));
    printf("environ=0x%lx\n", (long)environ);

    if (! getenv("FOO")) {
	fprintf(stderr, "Error: no FOO EV defined\n");
	exit(1);
    }
    printf("FOO=%s\n", getenv("FOO"));

    env = environ;
    while (*env) {
	if (!strncmp(*env, "FOO=", 4)) {
	    char *value = *env + 4;
	    printf("orig=%s\n", value);
	    *value = 'X';
	    printf("patched=%s\n", value);
	    printf("*env=%s\n", *env);
	    //putenv(*env);
	}
	env++;
    }

    printf("FOO=%s\n", getenv("FOO"));

    exit(0);
}
