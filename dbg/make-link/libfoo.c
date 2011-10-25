/*
 * Build: gcc -g -W -Wall -fpic -D_REENTRANT -shared -o ldlab.so ldlab.c
 *
 * Run:   LD_PRELOAD=./ldlab.so uname
 *
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static void ldlab_init(void) __attribute__ ((constructor));	// gcc only

static int count;

static void
ldlab_init(void)
{
    write(STDERR_FILENO, "= Starting\n", 11);
}

void
do_something(void)
{
    count++;
}
