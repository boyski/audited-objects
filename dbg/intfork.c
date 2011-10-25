// To compile:
//   gcc -W -Wall -fpic -shared intfork.c -o intfork.so
// or (Sun compiler):
//   cc -G -xcode=pic13 intfork.c -o intfork.so

// Optional: -z interpose -z combreloc -z ignore -z lazyload

// To use:
//   LD_PRELOAD=./intfork.so <program> <args>...

#if defined(linux)
#define  _GNU_SOURCE
#endif	/*linux*/

#include <dlfcn.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#define FUN_PROTO(type,internal,parms) type internal parms
#define DECLARE(type,name, parms) static FUN_PROTO(type,(*name), parms)
#define CAST(type, parms) (FUN_PROTO(type,(*), parms))

#ifdef __GNUC__
void    onload(void) __attribute__((constructor));
#else	/*__GNUC__*/
#pragma init(onload)
#endif	/*__GNUC__*/

DECLARE(pid_t, next_fork,  (void));
DECLARE(pid_t, next_vfork, (void));
#if defined(WRAP_CLONE)
DECLARE(int,   next_clone, (int(*)(void *), void *, int, void *));
#endif	/*WRAP_CLONE*/

void
onload(void)
{
    next_fork =  CAST(pid_t, (void)) dlsym(RTLD_NEXT, "fork");
    next_vfork = CAST(pid_t, (void)) dlsym(RTLD_NEXT, "vfork");
#if defined(WRAP_CLONE)
    next_clone = CAST(int, (int(*)(void *), void *, int, void *)) dlsym(RTLD_NEXT, "clone");
#endif	/*WRAP_CLONE*/
}

pid_t
fork(void)
{
    fprintf(stderr, "= intercepted fork in %ld\n", (long)getpid());
    return next_fork();

#if 0
    if (pid) {
	fprintf(stderr, "= fork returned %ld (parent)\n", (long)pid);
    } else {
	fprintf(stderr, "= fork returned %ld (child)\n", (long)pid);
    }
    return pid;
#endif
}

pid_t
vfork(void)
{
    fprintf(stderr, "= intercepted vfork in %ld\n", (long)getpid());
    return next_fork();

#if 0
    pid = next_vfork();
    if (pid) {
	fprintf(stderr, "= vfork returned %ld (parent)\n", (long)pid);
    } else {
	fprintf(stderr, "= vfork returned %ld (child)\n", (long)pid);
    }
    return pid;
#endif
}
