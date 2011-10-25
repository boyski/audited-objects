#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

int
open_wrapper(const char *call,
	     int (*next) (const char *, int, mode_t),
	     const char *path, int oflag, mode_t mode)
{
    int ret;

    fprintf(stderr, "Interposed on %s()\n", call);
    ret = (*next)(path, oflag, mode);

    return ret;
}

int
open(const char *path, int oflag, ...)
{
    mode_t mode = 0777;
    int ret;
    static int (*original) ();

    if (!original && !(original = (int (*)())dlsym(RTLD_NEXT, "open"))) {
	fprintf(stderr, "Warning: no original found for %s()", "open");
    }

    ret = open_wrapper("open", original, path, oflag, mode);
    return ret;
}
