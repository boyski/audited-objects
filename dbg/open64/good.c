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
open(const char *path, int oflag, ...)
{
    static int(*next)(const char *, int, ...);
    mode_t mode = 0;
    int ret;

    if (!next) {
	next = dlsym(RTLD_NEXT, "open");
    }

    if (oflag & O_CREAT) {
	va_list ap;

	va_start(ap, oflag);
	mode = va_arg(ap, int);  /* NOT mode_t */
	va_end(ap);
    } else {
	mode = 0;
    }

    ret = next(path, oflag, mode);
    fprintf(stderr, "= %05d: open(%s, %d, 0%o) = %d\n",
	    (int)getpid(), path, oflag, (unsigned)mode, ret);
    return ret;
}
