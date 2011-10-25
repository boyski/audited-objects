/*
 * Build: gcc -g -W -Wall -fpic -D_REENTRANT -shared -o liblab.so liblab.c
 * gcc -m32 -DLIB=\"./liblab32.so\" -g -W -Wall -fPIC -D_REENTRANT -D_LARGEFILE_SOURCE -shared -nodefaultlibs -Bdirect -z interpose -z ignore -o liblab32.so liblab.c -ldl
 *
 * Run:   LD_PRELOAD=./liblab.so uname
 * or:    LD_PRELOAD_32=./liblab.so uname
 * or:    LD_PRELOAD_64=./liblab.so uname
 *
*/

#define _GNU_SOURCE

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

// TODO - is there a way for a library to figure out its own path
// within the filesystem?

#define LD_PRELOAD	"LD_PRELOAD"

static void liblab_init(void) __attribute__ ((constructor));	// gcc only
static void liblab_fini(void) __attribute__ ((destructor));	// gcc only

static void
liblab_init(void)
{
    fprintf(stderr, "= %05d: Implicitly loading %s ...\n",
	    (int)getpid(), LIB);
}

int
open(const char *path, int oflag, ...)
{
    static int(*next)(const char *, int, ...);
    mode_t mode = 0;
    int ret;

    if (!next) {
	next = dlsym(RTLD_NEXT, "open");
	fprintf(stderr, "@@ open_next=%p unlink=%p\n", next, dlsym(RTLD_NEXT, "unlink"));
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

#if !defined(__LP64__)
int
open64(const char *path, int oflag, ...)
{
    static int(*next)(const char *, int, ...);
    mode_t mode = 0;
    int ret;

    if (!next) {
	next = dlsym(RTLD_NEXT, "open64");
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
    fprintf(stderr, "= %05d: open64(%s, %d, 0%o) = %d\n",
	    (int)getpid(), path, oflag, (unsigned)mode, ret);
    return ret;
}
#endif

int
close(int fd)
{
    static int(*next)(int);
    int ret;

    if (!next) {
	next = dlsym(RTLD_NEXT, "close");
    }

    ret = next(fd);
    fprintf(stderr, "= %05d: close(%d) = %d\n", (int)getpid(), fd, ret);
    return ret;
}

static void
liblab_fini(void)
{
    fprintf(stderr, "= %05d: Implicitly unloading %s ...\n",
	    (int)getpid(), LIB);
}
