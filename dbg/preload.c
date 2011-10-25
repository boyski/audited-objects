/*
 * Compilation:
 *
 * (Solaris/Sun Studio):  cc -g -G -xcode=pic32 -D_REENTRANT
 * -D_LARGEFILE_SOURCE -o preload.so preload.c -ldl -lc
 *
 * GCC: gcc -g -W -Wall -fpic -D_REENTRANT -D_LARGEFILE_SOURCE -shared -o preload.so preload.c -ldl
*/

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <unistd.h>

#if defined(__SUNPRO_C)
static void preload_init(void);
static void preload_fini(void);
#pragma init (preload_init)
#pragma fini (preload_fini)
#else
static void preload_init(void) __attribute__ ((constructor));	// gcc only
static void preload_fini(void) __attribute__ ((destructor));	// gcc only
#endif

static const char *prg = "preload.so";

static void
preload_init(void)
{
    write(fileno(stderr), "= Initializing '", 16);
    write(STDERR_FILENO, prg, strlen(prg));
    write(STDERR_FILENO, "'\n", 2);
}

static void
finish(void)
{
    fprintf(stderr, "= Finalizing '%s'\n", prg);
}

static void
preload_fini(void)
{
    finish();
}
