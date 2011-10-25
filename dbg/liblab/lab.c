/*
 * Build: gcc -g -W -Wall -o lab lab.c
*/

#define _GNU_SOURCE

#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

int
main(void)
{
    void *handle = NULL;
    int fd;

    // This is called before the dlopen and should not be tracked
    // unless LD_PRELOAD was used.
    close(250);

    // Map in the library if not already mapped in via LD_PRELOAD.
    // TODO - this breaks if LD_PRELOAD exists and lists other libs.
    if (!getenv("LD_PRELOAD")) {
	if (!(handle = dlopen(LIB, RTLD_LAZY))) {
	    fprintf(stderr, "Error: %s: %s\n", LIB, dlerror());
	}
    }

    // The open and close here should be tracked with or without LD_PRELOAD.
    if ((fd = open("/etc/passwd", O_RDONLY)) == -1) {
	perror("/etc/passwd");
	exit(2);
    } else {
	close(fd);
    }

    // Same here since LD_PRELOAD should have been added if not present.
    (void)system("set -x; /bin/grep ABCDEFG /etc/group");

    // Unload the library iff it was loaded explicitly.
    if (handle) {
	dlclose(handle);
    }

    // This is called after the library is gone and should not be tracked
    // unless LD_PRELOAD was used.
    close(251);

    // Same here.
    (void)system("/bin/grep WXYZ /etc/motd");

    return 0;
}
