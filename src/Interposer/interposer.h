// Copyright (c) 2002-2011 David Boyce.  All rights reserved.

/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * NOTES:
 * -> Wrapping fcntl() is hard because it's not only variadic but
 * the type of the optional arg varies between int and pointer.
 * AFAIK the only case where we might even want to intercept it is
 * that of F_DUP2FD which is a synonym for dup2(), and in most cases
 * the only reason to hook dup2 functionality is to track open file
 * descriptors. For now, just assume that uses of fcntl(F_DUP2FD)
 * (as apposed to dup2) are rare. And most likely the only cost of
 * missing one is a small memory leak anyway.
 *
 * -> The vfork hack is still a nuisance. Best I can think of is to
 * write some assembly code, but maybe gcc has an extension that
 * would help.
 *
 * -> Can't currently wrap dl*(), specifically dlopen, on BSD variants.
 * See the comment in /usr/src/lib/libc/gen/dlfcn.c. But at least
 * at time of writing we are not trying to wrap dlopen anyway.
 * At one time we did and it worked, mostly, but it was complex
 * and not now believed to be worth the trouble.
 */

#ifndef _INTERPOSER_H
#define	_INTERPOSER_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

// The 'BSD' macro on BSD-like systems comes from <sys/param.h>
#include <sys/param.h>

// OSX doesn't allow direct access to 'environ' so we fake it here.
#if defined(__APPLE__)
#if !defined(environ)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#endif
#else
extern char **environ;
#endif

// Cause these to be called at shared-library load and unload time.
#if defined(__SUNPRO_C)
static void interposer_early_init(void);
static void interposer_finalize(void);
#pragma init (interposer_early_init)
#pragma fini (interposer_finalize)
#else
// These are gcc-only.
static void interposer_early_init(void) __attribute__ ((constructor));
static void interposer_finalize(void) __attribute__ ((destructor));
#endif

// There are a number of macros indicating 32- vs 64-bit mode.
// I do not know which ones are standard/reliable/portable, so
// what we do here is check for all known macros and turn them
// into one of our own creation. Some may be specific to a compiler
// (gcc vs Sun), some to an OS (Linux vs Windows), some to a
// particular CPU (AMD vs Intel vs Sparc). Note that FreeBSD
// is always 64-bit.
#if defined(__LP64__) || defined(_LP64) \
    || defined(__arch64__) || defined(__amd64__) \
    || defined(__sparcv9) || defined(__x86_64) || defined(__x86_64__) \
    || defined(__FreeBSD__)
#define INTERPOSER_64BIT_MODE
#endif

extern int interposer_set_dbg_flag(int);
extern int interposer_get_active(void);
extern int interposer_set_active(int);
extern const char *interposer_get_cmdline(void);
extern void *interposer_get_real(const char *);

// These are present as a check that the including
// code has the declaration correct.
extern void interposed_late_init(const char *);
#define	INTERPOSED_FINALIZE		"interposed_finalize"
extern void interposed_finalize(void);

// This is pretty ugly ... the openat() and related *at()
// functions were introduced in Solaris 9. In order to support
// earlier releases we must build on those older versions, but
// if we do so the resulting binary will have a major bug on Sol 9
// and above because many utilities use *at(). So we copy related
// macros from Sol 9's <fcntl.h> and hope they won't change values.
// It's ok to override functions on a system which doesn't have them.
#include <fcntl.h>
#ifdef sun
#ifndef	AT_FDCWD
#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__) || defined(_ATFILE_SOURCE)
#define	AT_FDCWD			0xffd19553
#define	AT_SYMLINK_NOFOLLOW		0x1000
#define	AT_REMOVEDIR			0x1
#endif
#endif	/*AT_FDCWD*/
#endif

// RTLD_SELF is the best way to look up functions within this
// module but some systems (Linux) don't have it.
#if defined(RTLD_SELF)
#define _RTLD_LOCAL_			RTLD_SELF
#else
#define _RTLD_LOCAL_			RTLD_DEFAULT
#endif

// This seems to work OK on Cygwin. Not sure why though.
#if defined(__CYGWIN__) && !defined(RTLD_NEXT)
#define RTLD_NEXT RTLD_DEFAULT
#endif

#if defined(INTERPOSER_DEBUG) || 0
#define WRAPPER_DEBUG(fmt, ...)						\
    fprintf(stderr, fmt, ## __VA_ARGS__)
#else
#define WRAPPER_DEBUG(fmt, ...)
#endif

/***********************************************************************
 *	WRAP* macros.
 **********************************************************************/

#define WRAP_PROLOG(n_type, n_call)					\
    static n_type (*stooge)();						\
    static n_type (*poseur)() = (n_type(*)())-1;			\
    if (interposer_early_initialized && !interposer_late_initialized)	\
	interposer_late_init(#n_call);					\
    if (!stooge) {							\
	stooge = (n_type(*)())dlsym(RTLD_NEXT, #n_call);		\
	if (!stooge) {							\
	    char *msg = (char *)dlerror();				\
	    fprintf(stderr, "Error: no symbol '%s': %s",		\
		#n_call, msg ? msg : "");				\
	    exit(2);							\
	}								\
    }									\
    if ((long)poseur == -1 && interposer_is_active) {			\
	poseur = (n_type(*)())dlsym(_RTLD_LOCAL_, #n_call "_wrapper");	\
	if (!poseur) {							\
	    if (interposer_dbg_flag > 1) {				\
		fprintf(stderr, "Error: no wrapper found for %s()",	\
		    #n_call);						\
		exit(2);						\
	    } else if (interposer_dbg_flag == 1) {			\
		fprintf(stderr, "Warning: no wrapper found for %s()",	\
		    #n_call);						\
	    }								\
	}								\
    }

#define WRAP(n_type, n_call, n_parms, ...)				\
n_type n_call n_parms {							\
    WRAP_PROLOG(n_type, n_call)						\
    if (poseur && interposer_is_active) {				\
	return poseur(#n_call, stooge, ##__VA_ARGS__);			\
    } else {								\
	return stooge(__VA_ARGS__);					\
    }									\
}

// Special case due to variadic arg list.
#define WRAP_OPEN(n_type, n_call)					\
n_type n_call (const char *path, int oflag, ...) {			\
    mode_t mode = 0;							\
    WRAP_PROLOG(n_type, n_call)						\
    if (oflag & O_CREAT) {						\
	va_list ap;							\
	va_start(ap, oflag);						\
	mode = va_arg(ap, int);	 /* NOT mode_t */			\
	va_end(ap);							\
    }									\
    if (poseur && interposer_is_active) {				\
	return poseur(#n_call, stooge, path, oflag, mode);		\
    } else {								\
	return stooge(path, oflag, mode);				\
    }									\
}

// Like open() but AFAIK this is Solaris-only.
#define WRAP_OPENAT(n_type, n_call)					\
n_type n_call (int fildes, const char *path, int oflag, ...) {		\
    mode_t mode = 0;							\
    WRAP_PROLOG(n_type, n_call)						\
    if (oflag & O_CREAT) {						\
	va_list ap;							\
	va_start(ap, oflag);						\
	mode = va_arg(ap, int);	 /* NOT mode_t */			\
	va_end(ap);							\
    }									\
    if (poseur && interposer_is_active) {				\
	return poseur(#n_call, stooge, fildes, path, oflag, mode);	\
    } else {								\
	return stooge(fildes, path, oflag, mode);			\
    }									\
}

#define WRAP_VOID(n_type, n_call)					\
n_type n_call (void) {							\
    WRAP_PROLOG(n_type, n_call)						\
    if (poseur && interposer_is_active) {				\
	return poseur(#n_call, stooge);					\
    } else {								\
	return stooge();						\
    }									\
}

// The execl*() functions are difficult to wrap, as are all variadic
// functions. The solution here is to convert each execl*() to the
// corresponding execv*() and call that. This is a little invasive,
// as it requires a malloc, but OTOH if the exec succeeds the heap
// is gone anyway and if it fails we might be exiting. For this
// reason there's typically no need for client code to wrap execl*().
#define WRAP_EXECL(n_type, n_style, n_need_envp)			\
n_type execl##n_style (const char *path, const char *arg0, ...) {	\
    va_list ap;								\
    const char *targv[8192];						\
    char **margv;							\
    char *const *envp = NULL;						\
    int maxarg = sizeof(targv)/sizeof(*targv) - 1;			\
    int erc;								\
    int i = 0;								\
    WRAP_PROLOG(n_type, execv##n_style)					\
    va_start(ap, arg0);							\
    targv[i++] = arg0;							\
    while ((targv[i] = (char *)va_arg(ap, char *)) && i < maxarg)	\
	i++;								\
    targv[i] = NULL;							\
    if (n_need_envp)							\
	envp = (char **)va_arg(ap, char **);				\
    va_end(ap);								\
    if (!(margv = malloc((i + 1) * sizeof(*targv)))) {			\
	perror("malloc()");						\
	exit(2);							\
    }									\
    for (i = 0; (margv[i] = (char *)targv[i]); i++);			\
    if (poseur && interposer_is_active) {				\
	if (n_need_envp) {						\
	    erc = poseur("execl"#n_style, stooge, path, margv, envp);	\
	} else {							\
	    erc = poseur("execl"#n_style, stooge, path, margv);		\
	}								\
    } else {								\
	if (n_need_envp) {						\
	    erc = stooge(path, margv, envp);				\
	} else {							\
	    erc = stooge(path, margv);					\
	}								\
    }									\
    free(margv);							\
    return erc;								\
}

#define WRAP_NORETURN(n_type, n_call, n_parms, ...)			\
n_type n_call n_parms {							\
    WRAP_PROLOG(n_type, n_call)						\
    if (poseur && interposer_is_active) {				\
	poseur(#n_call, stooge, ##__VA_ARGS__);				\
    } else {								\
	stooge(__VA_ARGS__);						\
    }									\
    while (1);								\
}

#ifdef  __cplusplus
}
#endif

/***********************************************************************
 *	Inlined static code
 **********************************************************************/

static int interposer_is_active;
static int interposer_early_initialized;
static int interposer_late_initialized;
static int interposer_is_finalized;
static int interposer_dbg_flag;
static struct timeval interposer_start_time;

// We work hard to determine the original argv passed to the program.
// This represents our best effort. Unfortunately, some programs
// overwrite their own argv during execution (e.g. "find ... -exec ..."
// on Solaris), so we can't just hold a pointer to it because the data
// might be useless by the time we need it. We must make a copy
// at startup time. Even more unfortunately, we can't malloc the copy
// because our constructor runs before that of libc, so malloc isn't
// available yet. And our next chance may be too late. So the best we
// can do is a big static buffer. ARG_MAX is normally plenty since the
// actual limit is (ARG_MAX - <size-of-environment> - <some-other-stuff>).
// However, if kernel params have been tweaked to set the limit
// higher than the compile-time constant, this could still truncate.
static char interposer_cmdline[ARG_MAX] = "???";

const char *
interposer_get_cmdline(void)
{
    return interposer_cmdline;
}

struct timeval *
interposer_get_start_time(void)
{
    if (!interposer_start_time.tv_sec)
	gettimeofday(&interposer_start_time, (struct timezone *)NULL);
    return &interposer_start_time;
}

int
interposer_set_dbg_flag(int dbg)
{
    int prev = interposer_dbg_flag;
    interposer_dbg_flag = dbg;
    return prev;
}

int
interposer_get_active(void)
{
    return interposer_is_active;
}

int
interposer_set_active(int active)
{
    int prev = interposer_is_active;
    interposer_is_active = active;
    return prev;
}

static char *
interposer_requote(char **argv, char *cmdline)
{
    char *word, *ptr;
    int quote;

    // There are 4 viable POSIX quoting strategies: (1) use only
    // backslashes (no quotes), (2) use double quotes and escape
    // as needed, (3) use single quotes and special-case
    // embedded single quotes, (4) use a hybrid strategy of examining
    // the word and choosing the simplest of single, double, or none.
    // The following represents a hybrid approach.

    for (ptr = cmdline; (word = *argv); argv++) {
	//fprintf(stderr, "--%s--\n", word);

	// If the word needs quoting at all, use single quotes unless
	// it _contains_ a single quote, in which case use double
	// quotes and escape special chars.
	if (*word) {
	    char *p;

	    for (quote = 0, p = word; *p; p++) {
		if (!isalnum((int)*p) && !strchr("!%+,-./=:@_", *p)) {
		    if (*p == '\'') {
			quote = '"';
			break;
		    } else {
			quote = '\'';
		    }
		}
	    }
	} else {
	    quote = '"';
	}

	// Add opening quote if required.
	if (quote) {
	    *ptr++ = quote;
	}

	// Copy the word into the output buffer with appropriate escapes
	// if using double-quotes.
	for (; *word; word++) {
	    if (quote == '"') {
		if (strchr("$\"`\\", *word)) {
		    *ptr++ = '\\';
		}
	    }
	    *ptr++ = *word;
	}

	// Add closing quote if required.
	if (quote) {
	    *ptr++ = quote;
	}

	// Append space or terminating null.
	if (argv[1]) {
	    *ptr++ = ' ';
	} else {
	    *ptr = '\0';
	}
    }

    //fprintf(stderr, "===%s===\n", cmdline);
    return cmdline;
}

static char **
interposer_argv_from_environ(int *p_argc)
{
#ifdef	_WIN32
    return NULL;
#else	/*!_WIN32*/

#if defined(__APPLE__)
    // OSX has its own way (as usual).
    char **av;
    av = *_NSGetArgv();
    if (p_argc) {
	int i;
	for (i = 0; av[i]; i++)
	*p_argc = i;
    }
    return av;
#elif defined(__CYGWIN__)
    extern char **__argv;
    char **av;
    av = __argv;
    if (p_argc) {
	int i;
	for (i = 0; av[i]; i++)
	*p_argc = i;
    }
    return av;
#else	/*!__APPLE__*/
    static char **orig_environ;
    char **p;

    // In case we end up getting called 'late', make sure to remember the
    // original location of the environment block.
    if (! orig_environ)
	orig_environ = environ;
    
    p = orig_environ - 2;

    if (*p) {
	// For this value we need a number smaller than a pointer but
	// larger than any possible argc value. Since ARG_MAX must
	// include the pointer to the argument as well as the argument
	// itself, the number of pointers which would fit into ARG_MAX
	// bytes is a conservative estimate.
	unsigned long not_ptr = ARG_MAX/sizeof(char *);
	unsigned long p2i;
	do {
	    p2i = (unsigned long) *--p;
	} while (p2i > not_ptr);
	if (p_argc)
	    *p_argc = (int)p2i;
	p++;
#if defined(sun) && defined(__arch64__)
	// This assignment was inserted because we were coming up with
	// the wrong value in a 64-bit Solaris process. I don't know
	// why that was happening but this seems to fix it.
	// The specific problem had to do with linking a 64-bit shared
	// library using gcc. The 32-bit linker is invoked and then
	// re-invokes itself in its 64-bit form. Somehow argv[0] in
	// the 64-bit version was identical to that in the 32-bit process.
	// Turns out this caused a new bug since programs which
	// are exec-ed via different symlinks and which figure out what
	// to do by examining the name they were run as will break
	// under AO since getexecname() resolves the symlink!
	// Of course the damage is now limited to 64-bit processes,
	// but we should debug the original problem since this workaround
	// has a serious flaw. See dbg/multilink for a test case; it
	// will do the right thing on Solaris when building 32-bit but
	// not when run in 64-bit mode (and this hack is removed).
	// UPDATE - I went back to try and fix this bug but could
	// not reproduce it, so for now I am leaving the hack present
	// but commented out. Possibly it was a Solaris bug now
	// fixed, or sparc only, or my test case is broken. A good
	// place to look if a 64-bit Solaris bug re-emerges.
	// p[0] = (char *)getexecname();
#endif	/*!sun*/
	return p;
    } else {
	return NULL;
    }
#endif	/*!__APPLE__*/
#endif	/*!_WIN32*/
}

static void interposer_late_init(const char *);

// Called automatically at library load time.
// Operates under severe restrictions: cannot use stdio or malloc
// since libc has not yet been initialized. Do as little as possible
// here.
static void
interposer_early_init(void)
{
    // If we can find an argv via this devious trick, copy it into
    // a static buffer for later use. We check that the buffer isn't
    // already filled in because we don't want to do this again on
    // the child side of a fork.
    // Must be done pre-main because many programs modify argv.
    // Even some option parsers (e.g. GNU getopt) do so.
    if (interposer_cmdline[0] == '?') {
	char **mav;

	if ((mav = interposer_argv_from_environ(NULL))) {
	    (void)interposer_requote(mav, interposer_cmdline);
	}
    }

    // This is best gotten "early" because an untold amount of
    // time could pass before the first intercepted lib call.
    (void)interposer_get_start_time();

    interposer_early_initialized = 1;

    interposer_late_init("init");
}

// Called the first time any wrapped function is called. Therefore
// definitely after main(), or more specifically after libc.so has
// been loaded and initialized, so we can count on things like stdio
// and malloc which may not be reliable in the early_init() function.
static void
interposer_late_init(const char *call)
{
    if (!interposer_early_initialized)
	return;
    if (interposer_late_initialized++)
	return;

    // The "receiver" lib must have its own initializer. Call it now.
    // Interception is off by default and must be enabled here.
    interposed_late_init(call);
}

// Called automatically at library unload time. No special restrictions.
static void
interposer_finalize(void)
{
    void (*final)(void);

    if (!interposer_is_finalized) {		// belt and suspenders
	interposer_is_finalized = 1;

	// A trivial program may reach this point without having
	// made any calls which would trigger the late initializer,
	// so make sure it's been run. [Turned off because we're now
	// wiring late-init into early-init].
	//if (!interposer_late_initialized)
	    //interposer_late_init("finalize");

	// The "receiver" lib may have its own finalizer.
	if (interposer_is_active) {
	    final = (void (*)(void))dlsym(_RTLD_LOCAL_, INTERPOSED_FINALIZE);
	    if (final)
		final();
	}
    }
}

void *
interposer_get_real(const char *call)
{
    void *real;

    // Let the receiver decide how to handle missing symbols.
    real = dlsym(RTLD_NEXT, call);

    return real;
}

// We must always intercept the chdir call, even when no explicit
// wrapper is needed. The reason is that certain things done at
// late-init time, most obviously calculation of absolute paths,
// can be confused by a cd. But if we intercept chdir we can do that
// work before the cwd is changed.
WRAP(int, chdir, (const char *path), path)

#endif	/* _INTERPOSER_H */
