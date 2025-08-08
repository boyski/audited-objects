// Copyright (c) 2002-2025 David Boyce.  All rights reserved.

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

#ifndef PUTIL_H
#define PUTIL_H

/// @file
/// @brief Portability utility functions, mostly for Unix/Windows support.

/// @cond RSN Functions in putil.h don't currently have dox.

/*
 * This is a "magic, three-way" header file. If included naively, i.e.
 * without any special PUTIL_* defines in place, it simply declares
 * a set of functions like any other C API header. However, when
 * included like this:
 *  #define PUTIL_DECLARE_FUNCTIONS_STATIC
 *  #include "putil.h"
 * It provides a full DEFINITION of each function, declared as static.
 * In other words the function bodies are "inlined" into the source file.
 * But when the PUTIL_DECLARE_FUNCTIONS_GLOBAL is defined instead, the
 * bodies are also inlined but now the functions are made extern,
 * meaning their symbols are global and available for linking from
 * other object modules.
 *
 * The PUTIL_DECLARE_FUNCTIONS_STATIC feature is for sharing useful
 * utility code without having to make a library. More importantly,
 * it allows such utility functions to be used in a shared library
 * without being exported. Unfortunately there is not a lot of scoping
 * power at the link level: a symbol is either local to the current
 * module (static) or is global and available to all (extern). Thus,
 * if we put utility functions in a library (.a) or even a regular
 * object module (.o) of their own, they'd be visible to users of
 * any shared library which uses them. By putting the entire function
 * in a header file we get code sharing without namespace pollution.
 * Meanwhile, to use these in a fully-bound executable where namespace
 * isn't such an issue, simply compile a tiny .c file containing the
 * following two lines:
 *  #define PUTIL_DECLARE_FUNCTIONS_GLOBAL
 *  #include "putil.h"
 */

/*
 * There are various rules to follow here:
 * 1. We try not to create any "FILE *" stdio structures here. There
 * are only 255 available in most Unix implementations and, as some
 * of this code may be grafted onto a "host process" which knows nothing
 * about it, we'd hate to take that process over a limit it would have
 * avoided without our help.
 * 2. Similarly, large mallocs or stack allocations are to be avoided
 * where possible. Host processes may be running near a limit and we
 * don't want to take them over it.
 */

/////////////////////////////////////////////////////////////////////////////

#if defined(PUTIL_DECLARE_FUNCTIONS_STATIC)
#define PUTIL_API   static
#define PUTIL_CLASS static
#else
#if !defined(PUTIL_DECLARE_FUNCTIONS_GLOBAL)
#define PUTIL_NO_BODY
#endif
#define PUTIL_API   extern
#define PUTIL_CLASS
#endif

/////////////////////////////////////////////////////////////////////////////

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ISASCII(c)      isascii((int)(c))

#define ISALPHA(c)      isalpha((int)(c))
#define ISUPPER(c)      isupper((int)(c))
#define ISLOWER(c)      islower((int)(c))
#define ISSPACE(c)      isspace((int)(c))
#define ISDIGIT(c)      isdigit((int)(c))
#define TOUPPER(c)      toupper((int)(c))
#define TOLOWER(c)      tolower((int)(c))

// For compatibility with Windows.
typedef int         SOCKET;

#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <unistd.h>
// The 'BSD' macro on BSD-like systems comes from <sys/param.h>
//#include <sys/param.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>

// Special hack because Linux, or at least Fedora, goes out of its
// way to hide/undef ARG_MAX presumably because such a limit no longer
// exists.  Might be a good idea to research this more deeply.
// For now, the number 131072 represents a pretty common traditional
// maximum but it's still a pretty random choice. Ideally we'd get
// out of the business of caring about ARG_MAX altogether.
#if !defined(ARG_MAX)
#define ARG_MAX         131072
#endif

#if defined(__APPLE__)
#include <crt_externs.h>
// OSX doesn't allow direct access to 'environ' so we fake it here.
#define environ (*_NSGetEnviron())
#else
extern char **environ;
#endif  /*__APPLE__*/

#define UNUSED(var)     (void)var

// Old Windows-related names.
typedef long LONG64;
typedef unsigned long ULONG64;

#ifndef O_BINARY
#define O_BINARY    0
#endif
#define __stat64    stat64
#define stricmp     strcasecmp
#define strnicmp    strncasecmp

// HOST_NAME_MAX is not universally available.
#if !defined(HOST_NAME_MAX)
#define HOST_NAME_MAX   512
#endif

// Borrowed from VMAC implementation. May be useful someday.
#if __GNUC__
#define PUTIL_ALIGN(n)      __attribute__ ((aligned(n))) 
#define PUTIL_NOINLINE      __attribute__ ((noinline))
#define PUTIL_FASTCALL
#elif _MSC_VER
#define PUTIL_ALIGN(n)      __declspec(align(n))
#define PUTIL_NOINLINE      __declspec(noinline)
#define PUTIL_FASTCALL      __fastcall
#define __attribute__(x)
#else
#define PUTIL_ALIGN(n)
#define PUTIL_NOINLINE
#define PUTIL_FASTCALL
#define __attribute__(x)
#endif

// This will free a pointer and then set the pointer to NULL.
// Helps make random failures more deterministic and turns use-after-free
// errors (which are generally subtle) into core dumps (unsubtle).
#define putil_free(ptr) { putil_free_(ptr); ptr = NULL; }

/////////////////////////////////////////////////////////////////////////////

#define putil_is_absolute(path)         (*path == '/')
#define putil_path_strcmp(s1, s2)       strcmp(s1, s2)
#define putil_path_strncmp(s1, s2, n)       strncmp(s1, s2, n)
#define putil_strtoll(p1, p2, n)        strtoll(p1, p2, n)
#define putil_strtoull(p1, p2, n)       strtoull(p1, p2, n)

/* Return the number of (potentially wide) characters in a buffer */
#define charlen(x)      (sizeof(x) / sizeof(char))

/* Return the number of *bytes* in a (potentially wide) string */
#define bytelen(x)      ((x) ? (strlen(x) * sizeof(char)) : 0)

/* Return the number of unused TCHARs in a null-terminated string buffer (for snprintf) */
#define leftlen(buf)        (charlen(buf) - strlen(buf))

/* Return the end of a null-terminated string */
#define endof(str)      strchr(str, '\0')

/* True if "str" ends with "ext" */
#define endswith(str, ext)  ((str) && (ext) && !strcmp(endof(str) - strlen(ext), ext))

// Windows-specific name hacks.
#define SOCKET                  int
#define INVALID_SOCKET              -1
#define SOCKET_ERROR                -1

/* Convenience: strcmp macro with reversed sense */
#define streq(a,b)  !strcmp(a,b)
#define strneq(a,b,c)   !strncmp(a,b,c)

/* Returns true when argument is a power of two. */
#define power_of_2(i)   (!(i & (i - 1)))

// For names of filesystems and similar small stuff
#define SMALLBUF 1024

/* Obvious but generally useful */
#define DEVNULL     "/dev/null"
#define SYS_SHELL   "/bin/sh"

// For general purpose automatic buffers. Typically this is to be used
// when we need a temporary construction buffer placed on the stack
// and will malloc what actually was constructed if need be.
#define BIGBUF      32768       // Windows max path is 32767

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// C++ compat, may be unnecessary.
#ifdef __cplusplus
extern "C" {
#endif

PUTIL_API void putil_strict_error(int);
PUTIL_API void putil_exit_(const char *, int, int);
PUTIL_API void putil_error_(const char *, int, const char *, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_int_(const char *, int, const char *, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_die_(const char *, int, const char *, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_warn_(const char *, int, const char *, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_srcdbg_(const char *, int, const char *, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_syserr_(const char *, int, int, const char *);
PUTIL_API void putil_lnkerr_(const char *, int, int, const char *, const char *);
PUTIL_API size_t putil_path_max(void);
PUTIL_API char *putil_basename(const char *);
PUTIL_API char *putil_dirname(const char *);
PUTIL_API int putil_mkdir_p(const char *);
PUTIL_API const char *putil_progname(void);
PUTIL_API char **putil_argv_from_environ(int *);
PUTIL_API const char *putil_getexecpath(void);
PUTIL_API char *putil_get_homedir(char *, size_t);
PUTIL_API char *putil_get_systemdir(char *, size_t);
PUTIL_API char *putil_realpath(const char *, int);
PUTIL_API char **putil_prepend2argv(char **, ...);
PUTIL_API size_t putil_searchpath(const char *, const char *, const char *, unsigned long, char *, char **);
PUTIL_API char *putil_path_strstr(const char *, const char *);
PUTIL_API void *putil_malloc(size_t);
PUTIL_API void *putil_calloc(size_t, size_t);
PUTIL_API void *putil_realloc(void *, size_t);
PUTIL_API void putil_free_(const void *);
PUTIL_API char *putil_strdup(const char *);
PUTIL_API char *putil_getenv(const char *);
PUTIL_API int putil_putenv(char *);
PUTIL_API void putil_unsetenv(const char *);
#define DIRSEP()    "/"
#define PATHSEP()   ":"
PUTIL_API const char *putil_tmpdir(char *, size_t);
PUTIL_API const char *putil_readlink(const char *);

#include <sys/utsname.h>
PUTIL_API int putil_uname(struct utsname *);

// C++ compat, may be unnecessary.
#ifdef __cplusplus
}
#endif

// We try not to use stdio functions anywhere in this file, because
// FILEs are limited to (256-3) in most Unix implementations. But of
// course stderr will have been pre-allocated. If it's been closed
// since then, not our problem.
#define _PUTIL_PRINTMSG_(keyword, f, l, fmt) \
{ \
    char *srcdbg; \
    va_list ap; \
    va_start(ap, fmt); \
    fprintf(stderr, "%s: %s: ", putil_progname(), #keyword); \
    if ((srcdbg = getenv("PUTIL_SRCDBG")) && atoi(srcdbg)) { \
    fprintf(stderr, "[at %s:%d] ", putil_basename(f), l); \
    } \
    (void) vfprintf(stderr, fmt, ap); \
    va_end(ap); \
    (void) fputc('\n', stderr); \
}

#define _PUTIL_PRINTMSGW_(keyword, f, l, fmt) \
{ \
    char *srcdbg; \
    va_list ap; \
    va_start(ap, fmt); \
    fprintf(stderr, "%s: %s: ", putil_progname(), #keyword); \
    if ((srcdbg = getenv("PUTIL_SRCDBG")) && atoi(srcdbg)) { \
    fprintf(stderr, "[at %s:%d] ", putil_basename(f), l); \
    } \
    (void) vfwprintf(stderr, fmt, ap); \
    va_end(ap); \
    (void) fputc('\n', stderr); \
}

#define _PUTIL_ABORT_() unlink("/cores/core");  abort()

#define _PUTIL_ERROR_DEBUG_(level, file, line) \
    if (Putil_Strict_Error < 0) { \
    _PUTIL_ABORT_(); \
    } else if (Putil_Strict_Error >= level) { \
    exit(2); \
    }

#define PUTIL_UNKNOWN_STR   "??"

// Note that variadic macros on Windows require MSVC 2005 or above.
#define putil_exit(status)  putil_exit_(__FILE__, __LINE__, status)
#define putil_error(...)    putil_error_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_int(...)      putil_int_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_die(...)      putil_die_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_warn(...)     putil_warn_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_srcdbg(...)   putil_srcdbg_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_syserr(...)   putil_syserr_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_lnkerr(...)   putil_lnkerr_(__FILE__, __LINE__, __VA_ARGS__)

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#ifndef PUTIL_NO_BODY

/////////////////////////////////////////////////////////////////////////////

#define _PUTIL_STR(x)  #x
#define _PUTIL_XSTR(x) _PUTIL_STR(x)

#if defined(PUTIL_DECLARE_FUNCTIONS_STATIC) || defined(PUTIL_DECLARE_FUNCTIONS_GLOBAL)
// This is a hack for allowing the application to register at runtime
// a preference for how to handle errors which are not necessarily
// fatal. A value of 0 means leave things as programmed; 1 means
// that all errors become fatal; 2 means that warnings also become fatal.
// -1 means to dump core on any error.
static int Putil_Strict_Error = 0;
#endif

PUTIL_CLASS void
putil_strict_error(int level)
{
    Putil_Strict_Error = level;
}

#undef _PUTIL_STR
#undef _PUTIL_XSTR

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_exit_(const char *f, int l, int status)
{
    if (f && l && status) {
    _PUTIL_ERROR_DEBUG_(3, f, l);
    }
    exit(status);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS size_t
putil_path_max(void)
{
    size_t path_max;

#ifdef PATH_MAX
    path_max = PATH_MAX;
#else
    path_max = pathconf(path, _PC_PATH_MAX);
    assert(path_max > 0);
#endif

    return path_max;
}

/////////////////////////////////////////////////////////////////////////////

// This API was originally based on the Win32 SearchPath function.
PUTIL_CLASS size_t
putil_searchpath(const char *srchpath, const char *prog, const char *ext,
                unsigned long buflen, char *buf, char **basepart)
{
    char *spath, *p, *e;
    size_t len = 0;
    int baselen;

    (void)ext;  // keep compiler happy - unix doesn't rely on extensions.
    
    baselen = strlen(prog) + 1;

    buf[0] = '\0';

    if (strchr(prog, '/')) {
        char *rbuf;

        if ((rbuf = putil_realpath(prog, 0))) {
            snprintf(buf, buflen, "%s", rbuf);
            putil_free(rbuf);
            return strlen(buf);
        } else {
            return 0;
        }
    }

    if (srchpath) {
        spath = putil_strdup(srchpath);
    } else {
        if (! (p = getenv("PATH"))) {
            return 0;
        }
        spath = putil_strdup(p);
    }

    for (p = spath; p; p = e + 1) {

        if ((e = strchr(p, ':')))
            *e = '\0';

        if (*p == '/') {
            strcpy(buf, p);
        } else if (*p == '.' || *p == '\0') {
            if (! getcwd(buf, PATH_MAX)) {
            putil_syserr(0, ".");
            continue;
            }
        } else {
            continue;
        }

        // If buffer not big enough, return required size.
        if (strlen(buf) + baselen > buflen) {
            len = strlen(buf) + baselen;
            buf[0] = '\0';
            break;
        }

        strcat(buf, "/");
        if (basepart) {
            *basepart = endof(buf);
        }
        strcat(buf, prog);

        if (!access(buf, X_OK)) {
            len = strlen(buf);
            break;
        }

        if (!e) {
            len = 0;
            buf[0] = '\0';
            break;
        }
    }

    putil_free(spath);

    return len;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_error_(const char *f, int l, const char *fmt, ...)
{
    _PUTIL_PRINTMSG_(Error, f,l, fmt);
    _PUTIL_ERROR_DEBUG_(2, f, l);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_int_(const char *f, int l, const char *fmt, ...)
{
    _PUTIL_PRINTMSG_(Internal Error, f, l, fmt);
    unlink("/cores/core");  // OS X doesn't like to overwrite core files
    abort();
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_die_(const char *f, int l, const char *fmt, ...)
{
    _PUTIL_PRINTMSG_(Error, f,l, fmt);
    _PUTIL_ERROR_DEBUG_(0, f, l);
    exit(2);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_warn_(const char *f, int l, const char *fmt, ...)
{
    _PUTIL_PRINTMSG_(Warning, f, l, fmt);
    _PUTIL_ERROR_DEBUG_(2, f, l);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_srcdbg_(const char *f, int l, const char *fmt, ...)
{
    char *t;

    if ((t = getenv("PUTIL_SRCDBG")) && atoi(t)) {
    _PUTIL_PRINTMSG_(Warning, f, l, fmt);
    _PUTIL_ERROR_DEBUG_(2, f, l);
    }
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_syserr_(const char *f, int l, int code, const char *string)
{
    char *msg;
    
    msg = strerror(errno);

    if (code) {
        if (string) {
            putil_error_(f,l, "%s: %s", string, msg);
        } else {
            putil_error_(f,l, "%s", msg);
        }
        if (getenv("AO_DEBUG")) {
            abort();
        }
        exit(code);
    } else {
        if (string) {
            putil_warn_(f,l, "%s: %s", string, msg);
        } else {
            putil_warn_(f,l, "%s", msg);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_lnkerr_(const char *f, int l, int code, const char *name1, const char *name2)
{
    char *msg;
    
    msg = strerror(errno);
    if (code) {
        putil_error_(f,l, "%s -> %s: %s", name2, name1, msg);
        exit(code);
    } else {
        putil_warn_(f,l, "%s -> %s: %s", name2, name1, msg);
    }
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS char *
putil_get_homedir(char *buf, size_t buflen)
{
    char *h;

    buf[0] = '\0';

    if ((h = getenv("HOME"))) {
        if (bytelen(h) <= buflen) {
            strcpy(buf, h);
            return buf;
        }
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS char *
putil_get_systemdir(char *buf, size_t buflen)
{
    (void)snprintf(buf, buflen, "/etc");
    return buf;
}

/////////////////////////////////////////////////////////////////////////////

// Similar semantics to realpath() except that if the path does not
// exist it may optionally return a best guess of what its absolute path
// *should* look like instead of failing as realpath() does
// on some systems.
// Note that this will resolve symlinks, just as SUS says realpath() does.
// Also note that it's just as insecure as the original.
PUTIL_CLASS char *
putil_realpath(const char *path, int guess)
{
    char resolved[PATH_MAX + 1];
    size_t len;

    len = sizeof(resolved);

    if (realpath(path, resolved)) {
        // Feeble attempt at making realpath more secure ...
        if (strlen(resolved) >= len)
            abort();
    } else {
        if (!guess) {
            return NULL;
        }

        if (errno != ENOENT) {
            return NULL;
        } else if (*path == '/') {
            char *parent;

            // This recursion should always break at "/" (the root).
            if ((parent = putil_dirname(path))) {
                char *rbuf;

                if ((rbuf = putil_realpath(parent, guess))) {
                    snprintf(resolved, leftlen(resolved), "%s/%s", rbuf, basename((char *)path));
                    putil_free(parent);
                    putil_free(rbuf);
                } else {
                    putil_free(parent);
                    return NULL;
                }
            }
        } else {
            if (getcwd(resolved, len - strlen(path) - sizeof(char))) {
                strcat(resolved, "/");
                strcat(resolved, path);
            } else {
                return NULL;
            }
        }
    }

    // Some realpath() implementations return NULL if path doesn't exist.
    if (!guess && access(resolved, F_OK)) {
        return NULL;
    }

    return putil_strdup(resolved);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS char *
putil_basename(const char *path)
{
    return basename((char *)path);
}

/////////////////////////////////////////////////////////////////////////////

// Like readlink(2) but allocates and returns a null-terminated buffer.
PUTIL_CLASS const char *
putil_readlink(const char *path)
{
    ssize_t len, l1;
    char *buf;

    if (!path) {
        return NULL;
    }

    len = strlen(path) + 1;
    buf = putil_malloc(len);

    while(1) {
        l1 = readlink(path, buf, len);
        if (l1 == -1) {
            putil_free(buf);
            buf = NULL;
            break;
        } else if (l1 == len) {
            len *= 2;
            buf = putil_realloc(buf, len);
        } else {
            buf[l1] = '\0';
            break;
        }
    }

    return buf;
}

/////////////////////////////////////////////////////////////////////////////

// The returned string needs to be freed manually.
PUTIL_CLASS char *
putil_dirname(const char *path)
{
    if (!path) {
        return NULL;
    }
    return dirname(putil_strdup(path));
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS int
putil_mkdir_p(const char *path)
{
    char *parent;

    if ((parent = putil_dirname(path))) {
        if (access(parent, F_OK)) {
            (void)putil_mkdir_p(parent);
        }
        putil_free(parent);
    }
    return mkdir(path, 0777);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS const char *
putil_progname(void)
{
    return basename((char *)putil_getexecpath());
}

/////////////////////////////////////////////////////////////////////////////

// Resort to the classic hack - assume argv immediately precedes environ
// in memory. Which implies that this function MUST be called before the
// environment is modified, as that may result in 'environ' being
// realloc-ed to a new location away from argv.
// Assume we've reached argc, which should be just past argv,
// when we find a value (p2i) smaller than any valid ptr.
// NOTE: I've seen some references on the web to __xargv. Not sure
// what platform, whose libc implements this. If any.
PUTIL_CLASS char **
putil_argv_from_environ(int *p_argc)
{
#if defined(__APPLE__)
    // OSX has its own way (as usual).
    char **av;
    av = *_NSGetArgv();
    if (p_argc) {
        int i;
        for (i = 0; av[i]; i++) {
            *p_argc = i;
        }
    }
    return av;
#else   /*__APPLE__*/
    static char **orig_environ;
    char **p;

    // In case we end up getting called late, make sure to remember the
    // original location of the environment block.
    if (! orig_environ) {
        orig_environ = environ;
    }
    
    p = orig_environ - 2;

    if (*p) {
        // For this value we need a number smaller than a pointer but
        // larger than any possible argc value. Since ARG_MAX must
        // include the pointer to the argument as well as the argument
        // itself, the number of pointers which would fit into ARG_MAX
        // bytes is a conservative estimate.
        ULONG64 not_ptr = ARG_MAX/sizeof(char *);
        ULONG64 p2i;
        do {
            p2i = (ULONG64) *--p;
        } while (p2i > not_ptr);
        if (p_argc) {
            *p_argc = (int)p2i;
        }
        return p+1;
    } else {
        return NULL;
    }
#endif  /*__APPLE__*/
}

/////////////////////////////////////////////////////////////////////////////

// Special rule for putil_getexecpath: if a program is going to
// use it, it should be called asap before the 'environ' array
// has a chance to get realloc-ed away from argv. This will cause
// the result to be stashed in a static buffer, so it may safely
// be used later as long as it was called early.
PUTIL_CLASS const char *
putil_getexecpath(void)
{
    static char xname[PATH_MAX];
    char buf[PATH_MAX + 1], pbuf[PATH_MAX];

    // This can't change once we derive it ...
    if (xname[0] != '\0') {
        return xname;
    }

    (void)memset(buf, 0, sizeof(buf));

// On Linux the "classic hack" doesn't always find the full path to
// the executable. So we try /proc first. If that doesn't work,
// John Reiser (jreiser@BitWagon.com) stated in comp.unix.programmer
// on 10/29/04 that:
//    "In Linux, the actual pathname passed as the first argument to
//    a successful execve() is recorded as an additional string beyond
//    the characters of the last environment variable.  So,
//    1+strlen(environ[last])+environ[last] is a string containing the
//    pathname that was given to execve()."
// So we look for something past the environ block that looks like a path.
#if defined(linux)
    {
        ssize_t llen;

        if ((llen = readlink("/proc/self/exe", buf, charlen(buf) - 1)) == -1) {
            char **e;
            char *p;

            for (e = environ; *(e + 1); e++)
                ;
            p = strchr(*e, '\0') + 1;
            if (*p == '/') {
                (void)strcpy(buf, p);
            } else {
                buf[0] = '\0';
                return NULL;
            }
        } else {
            buf[llen] = '\0';
        }
    }
#else
    if (! putil_is_absolute(buf)) {
        // At this point we've found we're on a system without a
        // 100% ironclad way of getting the exec path. Try digging
        // up the argv, which only works if argv immediately
        // precedes the environ block in memory, and seeing if we
        // can do something with argv[0]. Note that many Unix books,
        // FAQs, etc. deprecate this but it does usually "work".
        char **argv = putil_argv_from_environ(NULL);
        if (argv) {
            size_t len = strlen(argv[0]);

            (void)strncpy(buf, argv[0], len);
            buf[len] = '\0';
        } else {
            buf[0] = '\0';
            return NULL;
        }
    }

#endif

    // If we ended up with an absolute path we're done.
    if (putil_is_absolute(buf)) {
        return strcpy(xname, buf);
    }

    // Search PATH for it.
    if (putil_searchpath(NULL, buf, NULL, charlen(pbuf), pbuf, NULL)) {
        return strcpy(xname, pbuf);
    }

    // Last try - look for it in the CWD.
    if (!access(buf, X_OK)) {
        if (getcwd(xname, charlen(xname)) == NULL) {
            putil_syserr(1, "getcwd");
        }
        strcat(xname, "/");
        return strcat(xname, buf);
    }

    buf[0] = '\0';
    return NULL;
}

/////////////////////////////////////////////////////////////////////////////

// There's no strstr() function on Windows so synthesize one.
PUTIL_CLASS char *
putil_path_strstr(const char *s1, const char *s2)
{
    return strstr(s1, s2);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void *
putil_malloc(size_t size)
{
    void *ptr = malloc(size);

    if (ptr == NULL) {
        putil_syserr(2, "malloc");
    }
    return ptr;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void *
putil_calloc(size_t nelem, size_t elsize)
{
    void *ptr = calloc(nelem, elsize);
    if (ptr == NULL) {
        putil_syserr(2, "calloc");
    }
    return ptr;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void *
putil_realloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (ptr == NULL) {
        putil_syserr(2, "realloc");
    }
    return ptr;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_free_(const void *ptr)
{
    free((void *)ptr);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS char *
putil_strdup(const char *s1)
{
    size_t len = bytelen(s1);
    char *newstr = (char *)putil_malloc(len + sizeof(char));
    return strcpy(newstr, s1);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS char *
putil_getenv(const char *name)
{
    return getenv(name);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS int
putil_putenv(char *string)
{
    return putenv(string);
}

/////////////////////////////////////////////////////////////////////////////

// Modeled on the FreeBSD unsetenv(). This can waste env space
// by not using realloc to compress the env block to the new size,
// but it will usually be used just before an exec anyway - when
// else would you remove from the env? - and it's always possible the
// libc env code uses some internal allocator, so we leave it.
PUTIL_CLASS void
putil_unsetenv(const char *name)
{
    char **ep;
    size_t len;

    len = strlen(name);
    for (ep = environ; *ep; ++ep) {
        if (!strncmp(*ep, name, len) && ep[0][len] == '=') {
            char **np;

            for (np = ep; np[0]; np++) {
                np[0] = np[1];
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS int
putil_uname(struct utsname *name)
{
    return uname(name);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS char **
putil_prepend2argv(char **argv, ...)
{
    char **nargv, **np;
    char *str;
    int i;
    va_list ap;

    // Count the strings in argv.
    for (i = 0; argv[i]; i++)
        ;

    // Count the additional strings.
    va_start(ap, argv);
    for (; va_arg(ap, const char *); i++)
        ;
    va_end(ap);

    // Allocate the new argv
    nargv = np = (char **)putil_malloc((i + 1) * sizeof(nargv[0]));

    // Rewind 'ap' and put the extras at the front of the array ...
    va_start(ap, argv);
    while ((str = (char *) va_arg(ap, char *))) {
        *np++ = str;
    }
    va_end(ap);

    // ... followed by the argv ...
    while (*argv) {
        *np++ = *argv++;
    }

    // ... and a terminating null.
    *np = NULL;

    return nargv;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS const char *
putil_tmpdir(char *tbuf, size_t tlen)
{
    const char *t;

    if (!(t = getenv("TMPDIR"))) {
        t = "/tmp";
    }
    snprintf(tbuf, tlen, "%s/", t);
    return tbuf;
}

// Simply to help suppressing warnings. The space is there because
// otherwise we get a warning from __attribute__(( __format__)).
#define PUTIL_NULLSTR       " "
#define PUTIL_NULLSTRW      L" "

// This exists only to suppress a warning emitted by gcc when
// you define a static func but don't call it in the same
// compilation unit. This function calls itself recursively to try
// to avoid that same warning.
void _putil_never_called(void);
void
_putil_never_called(void)
{
    putil_strict_error(0);
    putil_exit(0);
    putil_error(" ");
    putil_int(PUTIL_NULLSTR);
    putil_die(PUTIL_NULLSTR);
    putil_warn(PUTIL_NULLSTR);
    putil_srcdbg(PUTIL_NULLSTR);
    putil_syserr(0, PUTIL_NULLSTR);
    putil_lnkerr(0, PUTIL_NULLSTR, PUTIL_NULLSTR);
    putil_tmpdir(PUTIL_NULLSTR, 0);
    putil_basename(PUTIL_NULLSTR);
    putil_dirname(PUTIL_NULLSTR);
    putil_prepend2argv(NULL);
    putil_get_homedir(PUTIL_NULLSTR, 0);
    putil_get_systemdir(PUTIL_NULLSTR, 0);
    putil_path_strstr(PUTIL_NULLSTR, PUTIL_NULLSTR);
    putil_malloc(0);
    putil_calloc(0, 0);
    putil_realloc(PUTIL_NULLSTR, 0);
    putil_strdup(PUTIL_NULLSTR);
    putil_free_(PUTIL_NULLSTR);
    putil_argv_from_environ(NULL);
    putil_getenv(PUTIL_NULLSTR);
    putil_putenv(PUTIL_NULLSTR);
    putil_unsetenv(PUTIL_NULLSTR);
    putil_uname(NULL);
    putil_mkdir_p(PUTIL_NULLSTR);
    putil_readlink(NULL);

#if !defined(__clang__)
    // Avoid warning via bogus recursive call.
    if (0) {
        _putil_never_called();
    }
#endif  /*__clang__*/
}

#undef PUTIL_NULLSTR
#undef PUTIL_NULLSTRW

#endif  /* PUTIL_NO_BODY */

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#undef PUTIL_API
#undef PUTIL_CLASS
#undef PUTIL_NO_BODY

/// @endcond RSN

#endif  /* PUTIL_H */

// vim: sw=4 ts=4 et tw=100 cc=+1
