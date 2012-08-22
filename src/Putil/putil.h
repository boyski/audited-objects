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
 *	#define PUTIL_DECLARE_FUNCTIONS_STATIC
 *	#include "putil.h"
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
 *	#define PUTIL_DECLARE_FUNCTIONS_GLOBAL
 *	#include "putil.h"
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
#define PUTIL_API	static
#define PUTIL_CLASS	static
#else
#if !defined(PUTIL_DECLARE_FUNCTIONS_GLOBAL)
#define PUTIL_NO_BODY
#endif
#define PUTIL_API	extern
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

#define ISASCII(c)		isascii((int)(c))

#define ISALPHA(c)		isalpha((int)(c))
#define ISUPPER(c)		isupper((int)(c))
#define ISLOWER(c)		islower((int)(c))
#define ISSPACE(c)		isspace((int)(c))
#define ISDIGIT(c)		isdigit((int)(c))
#define TOUPPER(c)		toupper((int)(c))
#define TOLOWER(c)		tolower((int)(c))

#if defined(_WIN32)
#pragma warning(disable:4706)		// Stupid *&#$@ warning!
#endif	/*_WIN32*/

#if !defined(_WIN32)
// For compatibility with Windows.
typedef int			SOCKET;
#endif

#if !defined(_WIN32)
#define DebugBreak()
#endif

#ifdef _WIN32
// We use GetVolumePathName*(), GetComputerNameEx(), and probably others.
#define _WIN32_WINNT 0x0500
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <malloc.h>
#include <process.h>
// As there's no <inttypes.h> for *#%$& Windows, fake the parts we need.
typedef ULONG32		uint32_t;
typedef __int64		int64_t;
typedef ULONG64		uint64_t;
#define	PRId64		"I64d"
#define	PRIu64		"I64u"
#else	/*_WIN32*/
#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <unistd.h>
// The 'BSD' macro on BSD-like systems comes from <sys/param.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#endif	/*_WIN32*/

// Special hack because Linux, or at least Fedora, goes out of its
// way to hide/undef ARG_MAX presumably because such a limit no longer
// exists.  Might be a good idea to research this more deeply.
// For now, the number 131072 represents the most recent max on
// both Solaris and Linux, and probably the maximum-maximum of
// all UNix platforms. But still a pretty random choice. Ideally
// we would also get out of the business of caring about ARG_MAX.
#if !defined(ARG_MAX)
#define ARG_MAX			131072
#endif

#if defined(__APPLE__)
#include <crt_externs.h>
// OSX doesn't allow direct access to 'environ' so we fake it here.
#define environ (*_NSGetEnviron())
#elif !defined(_WIN32)	/*!__APPLE__ && !_WIN32*/
extern char **environ;
#endif	/*__APPLE__*/

typedef char *CS;
typedef const char *CCS;

#ifndef PUTIL_BUILTON
#define PUTIL_BUILTON		"UNKNOWN "
#endif

#define UNUSED(var)		(void)var

#ifdef	BSD
// Different name, same thing.
#define st_mtim st_mtimespec
#endif	/*BSD*/

// FreeBSD is just 64-bit all the way, no transition environment.
// I don't know what's up with Cygwin but it also has no *64 interfaces.
#if defined(BSD) || defined(__CYGWIN__)
#define off64_t		off_t
#define lseek64		lseek
#define open64		open
#define stat64		stat
#define mmap64		mmap
#define lstat64		lstat
#define fstat64		fstat
#define O_LARGEFILE	0
#endif	/*BSD || _WIN32*/

// On Windows a long is the same size as an int (32 bits in Win32)
// while a pointer is 64 bits even on Win32. Don't ask me why.
// The upshot is that pointer arithmetic involving casts to long
// gives a warning on Windows. We use the following, borrowed from
// <windows.h>, to hack around this.
#if !defined(_WIN32)
typedef long LONG64;
typedef unsigned long ULONG64;
#endif	/*_WIN32*/

// Hacks for Win32 CRT
#if defined(_WIN32) && !defined(__GNUC__)
typedef int		socklen_t;
typedef unsigned long	mode_t;
typedef long		pid_t;
typedef long		ssize_t;
#define O_RDONLY	_O_RDONLY
#define O_WRONLY	_O_WRONLY
#define O_BINARY	_O_BINARY
#define	S_ISDIR(mode)	(((mode)&0xF000) == 0x4000)
#define	S_ISREG(mode)	(((mode)&0xF000) == 0x8000)
#define open64		_open
#define pclose		_pclose
#define popen		_popen
#define stat64		_stat64
#define lstat64		_stat64
#define fstat64		_fstat64
#else	/*!_WIN32 || __GNUC__*/
#ifndef O_BINARY
#define O_BINARY	0
#endif
#define __stat64	stat64		// the struct
#define stricmp		strcasecmp
#define strnicmp	strncasecmp
#endif	/*_WIN32 && !__GNUC__*/

// Windows supports access() but not these constants.
#if defined(_WIN32) && !defined(__GNUC__)
#define R_OK	04
#define W_OK	02
//#define X_OK	01	/* We need to know about places that use this */
#define F_OK	00

#if !defined(STDERR_FILENO)
#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2
#endif

// Confusion between size_t and unsigned due to different signatures
// of e.g. read() between POSIX and Windows. The pragma is specific
// to MSVC.
#pragma warning(disable: 4267)
// I don't see a problem with "upcasting" a size to a larger size ...
#pragma warning(disable: 4312)

#define PATH_MAX MAX_PATH

#endif	/*_WIN32 && !__GNUC__*/

// HOST_NAME_MAX is not universally available.
#if !defined(HOST_NAME_MAX)
#define HOST_NAME_MAX	512
#endif

// Borrowed from VMAC implementation. May be useful someday.
#if __GNUC__
#define PUTIL_ALIGN(n)		__attribute__ ((aligned(n))) 
#define PUTIL_NOINLINE		__attribute__ ((noinline))
#define PUTIL_FASTCALL
#elif _MSC_VER
#define PUTIL_ALIGN(n)		__declspec(align(n))
#define PUTIL_NOINLINE		__declspec(noinline)
#define PUTIL_FASTCALL		__fastcall
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
#define putil_free(ptr)	{ putil_free_(ptr); ptr = NULL; }

/////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#define putil_is_absolute(path)	    ((*path == '/' || *path == '\\'	\
				    || (ISALPHA(*path)			\
				    && path[1] == ':'			\
				    && (path[2] == '/' || path[2] == '\\'))))
#define putil_path_strcmp(s1, s2)		_stricmp(s1, s2)
#define putil_path_strncmp(s1, s2, n)		_strnicmp(s1, s2, n)
#define putil_strtoll(p1, p2, n)		_strtoi64(p1, p2, n)
#define putil_strtoull(p1, p2, n)		_strtoui64(p1, p2, n)
#else	/*_WIN32*/
#define putil_is_absolute(path)			(*path == '/')
#define putil_path_strcmp(s1, s2)		strcmp(s1, s2)
#define putil_path_strncmp(s1, s2, n)		strncmp(s1, s2, n)
#define putil_strtoll(p1, p2, n)		strtoll(p1, p2, n)
#define putil_strtoull(p1, p2, n)		strtoull(p1, p2, n)
#endif	/*_WIN32*/

/* Return the number of (potentially wide) characters in a buffer */
#define charlen(x)		(sizeof(x)/sizeof(char))

/* Return the number of wide characters in a buffer */
#define charlenW(x)		(sizeof(x)/sizeof(wchar_t))

/* Return the number of *bytes* in a (potentially wide) string */
#define bytelen(x)		((x) ? (strlen(x)*sizeof(char)) : 0)

/* Return the number of unused TCHARs in a string buffer (for snprintf) */
#define leftlen(buf)	(charlen(buf) - strlen(buf))

/* Return the end of a null-terminated string */
#define endof(str)	strchr(str, '\0')

#ifdef _WIN32
#define endswith(str, ext)	(!stricmp(endof(str) - strlen(ext), ext))
#else	/*_WIN32*/
#define endswith(str, ext)	(!strcmp(endof(str) - strlen(ext), ext))
#endif	/*_WIN32*/

// Allows Unix- and Windows-specific code segments to look more alike.
#if !defined(_WIN32)
#define SOCKET					int
#define INVALID_SOCKET				-1
#define SOCKET_ERROR				-1
#endif	/*!_WIN32*/

/* Convenience: strcmp macro with reversed sense */
#define streq(a,b)	!strcmp(a,b)
#define strneq(a,b,c)	!strncmp(a,b,c)

/* Returns true when argument is a power of two. */
#define power_of_2(i)	(!(i & (i - 1)))

// For names of filesystems and similar small stuff
#define SMALLBUF 1024

/* Obvious but generally useful */
#ifdef _WIN32
#define DEVNULL		"NUL"
#define SYS_SHELL	"cmd.exe"
#else	/*_WIN32*/
#define DEVNULL		"/dev/null"
#define SYS_SHELL	"/bin/sh"
#endif	/*_WIN32*/

// For general purpose automatic buffers. Typically this is to be used
// when we need a temporary construction buffer placed on the stack
// and will malloc what actually was constructed if need be.
#define BIGBUF		32768		// Windows max path is 32767

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

PUTIL_API CCS putil_builton(void);
PUTIL_API void putil_strict_error(int);
PUTIL_API void putil_exit_(CCS, int, int);
PUTIL_API void putil_error_(CCS, int, CCS, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_int_(CCS, int, CCS, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_die_(CCS, int, CCS, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_warn_(CCS, int, CCS, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_srcdbg_(CCS, int, CCS, ...)
    __attribute__((__format__(__printf__,3,4)));
PUTIL_API void putil_syserr_(CCS, int, int, CCS);
PUTIL_API void putil_lnkerr_(CCS, int, int, CCS, CCS);
PUTIL_API size_t putil_path_max(void);
PUTIL_API CS putil_basename(CCS);
PUTIL_API CS putil_dirname(CCS);
PUTIL_API int putil_mkdir_p(CCS);
PUTIL_API CCS putil_prog(void);
PUTIL_API CS *putil_argv_from_environ(int *);
PUTIL_API CCS putil_getexecpath(void);
PUTIL_API CS putil_get_homedir(CS, size_t);
PUTIL_API CS putil_get_systemdir(CS, size_t);
PUTIL_API CS putil_realpath(CCS, int);
PUTIL_API CS *putil_prepend2argv(CS *, ...);
PUTIL_API size_t putil_searchpath(CCS, CCS, CCS, unsigned long, CS, CS *);
PUTIL_API CS putil_canon_path(CS, CS, size_t);
PUTIL_API CS putil_path_strstr(CCS, CCS);
PUTIL_API void *putil_malloc(size_t);
PUTIL_API void *putil_malloc_str(size_t);
PUTIL_API void *putil_calloc(size_t, size_t);
PUTIL_API void *putil_realloc(void *, size_t);
PUTIL_API void putil_free_(const void *);
PUTIL_API CS putil_strdup(CCS);
PUTIL_API CS putil_getenv(CCS);
PUTIL_API int putil_putenv(CS);
PUTIL_API void putil_unsetenv(CCS);

#if defined(_WIN32)
PUTIL_API void putil_errorW_(CCS, int, WCHAR *, ...);
PUTIL_API void putil_warnW_(CCS, int, WCHAR *, ...);
PUTIL_API void putil_win32err_(CCS, int, int, DWORD, CCS);
PUTIL_API void putil_win32errW_(CCS, int, int, DWORD, LPCWSTR);
PUTIL_API void putil_syserrW_(CCS, int, int, WCHAR *);
PUTIL_API CCS putil_tmpdir(CS, DWORD);
PUTIL_API CCS DIRSEP(void);
PUTIL_API CCS PATHSEP(void);
#else	/*_WIN32*/
#define DIRSEP()	"/"
#define PATHSEP()	":"
PUTIL_API CCS putil_tmpdir(CS, size_t);
PUTIL_API CCS putil_readlink(CCS);
#endif	/*_WIN32*/

#if defined(_WIN32)
struct utsname {
    char sysname[257];
    char nodename[257];
    char release[257];
    char version[257];
    char machine[257];
};
#else	/*_WIN32*/
#include <sys/utsname.h>
#endif	/*_WIN32*/
PUTIL_API int putil_uname(struct utsname *);

#ifdef __cplusplus
}
#endif

// We try not to use stdio functions anywhere in this file, because
// FILEs are limited to (256-3) in most Unix implementations. But of
// course stderr will have been pre-allocated. If it's been closed
// since then, not our problem.
#define _PUTIL_PRINTMSG_(keyword, f, l, fmt)				\
{									\
    CS srcdbg;								\
    va_list ap;								\
    va_start(ap, fmt);							\
    fprintf(stderr, "%s: %s: ", putil_prog(), #keyword);		\
    if ((srcdbg = getenv("PUTIL_SRCDBG")) && atoi(srcdbg)) {		\
	fprintf(stderr, "[at %s:%d] ", putil_basename(f), l);		\
    }									\
    (void) vfprintf(stderr, fmt, ap);					\
    va_end(ap);								\
    (void) fputc('\n', stderr);						\
}

#define _PUTIL_PRINTMSGW_(keyword, f, l, fmt)				\
{									\
    CS srcdbg;								\
    va_list ap;								\
    va_start(ap, fmt);							\
    fprintf(stderr, "%s: %s: ", putil_prog(), #keyword);		\
    if ((srcdbg = getenv("PUTIL_SRCDBG")) && atoi(srcdbg)) {		\
	fprintf(stderr, "[at %s:%d] ", putil_basename(f), l);		\
    }									\
    (void) vfwprintf(stderr, fmt, ap);					\
    va_end(ap);								\
    (void) fputc('\n', stderr);						\
}

#if defined(_WIN32)
#define _PUTIL_ABORT_()	DebugBreak()
#else	/*_WIN32*/
#define _PUTIL_ABORT_()	unlink("/cores/core");	abort()
#endif	/*_WIN32*/

#define _PUTIL_ERROR_DEBUG_(level, file, line)				\
    if (Putil_Strict_Error < 0) {					\
	_PUTIL_ABORT_();						\
    } else if (Putil_Strict_Error >= level) {				\
	exit(2);							\
    }

#define PUTIL_UNKNOWN_STR	"??"

// Note that variadic macros on Windows require MSVC 2005 or above.
#define putil_exit(status)	putil_exit_(__FILE__, __LINE__, status)
#define putil_error(...)	putil_error_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_errorW(...)	putil_errorW_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_int(...)		putil_int_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_die(...)		putil_die_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_warn(...)		putil_warn_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_warnW(...)	putil_warnW_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_srcdbg(...)	putil_srcdbg_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_win32err(...)	putil_win32err_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_win32errW(...)	putil_win32errW_(__FILE__, __LINE__,__VA_ARGS__)
#define putil_syserr(...)	putil_syserr_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_syserrW(...)	putil_syserrW_(__FILE__, __LINE__, __VA_ARGS__)
#define putil_lnkerr(...)	putil_lnkerr_(__FILE__, __LINE__, __VA_ARGS__)

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#ifndef PUTIL_NO_BODY

/////////////////////////////////////////////////////////////////////////////

#define _PUTIL_STR(x)  #x
#define _PUTIL_XSTR(x) _PUTIL_STR(x)

#if defined(_MSC_VER)
#define PUTIL_COMPILER_	"MS Visual C " ## _PUTIL_XSTR(_MSC_VER)
#elif defined(__SUNPRO_C)
#define PUTIL_COMPILER_	"Sun Studio " ## _PUTIL_XSTR(__SUNPRO_C)
#elif defined(__GNUC__)
#define PUTIL_COMPILER_	"GCC " __VERSION__
#else
#define PUTIL_COMPILER_	"unknown compiler-version"
#endif	/*_WIN32 && !__GNUC__*/

PUTIL_CLASS char putil_platform[] = "@(#) " PUTIL_BUILTON "/" PUTIL_COMPILER_;

#if defined(PUTIL_DECLARE_FUNCTIONS_STATIC) || defined(PUTIL_DECLARE_FUNCTIONS_GLOBAL)
// This is a hack for allowing the application to register at runtime
// a preference for how to handle errors which are not necessarily
// fatal. A value of 0 means leave things as programmed; 1 means
// that all errors become fatal; 2 means that warnings also become fatal.
// -1 means to dump core on any error.
static int Putil_Strict_Error = 0;
#endif

PUTIL_CLASS CCS
putil_builton(void)
{
    return strchr(putil_platform, ' ') + 1;
}

PUTIL_CLASS void
putil_strict_error(int level)
{
    Putil_Strict_Error = level;
}

#undef _PUTIL_STR
#undef _PUTIL_XSTR

/////////////////////////////////////////////////////////////////////////////

// This is a function on Windows so it can return "/" in a Unix
// emulation environment - such as Cygwin - and "\" otherwise.
#if defined(_WIN32)
PUTIL_CLASS CCS
DIRSEP(void)
{
    static CCS dirsep;

    if (!dirsep) {
	dirsep = "\\";
	// This test was suggested for Cygwin but seems to have a good
	// hope of translating to other emulation environments. Of course
	// we could also define our own EV which would be dispositive.
	if (getenv("TERM")) {
	    dirsep = "/";
	}
    }
    return dirsep;
}
#endif	/*!_WIN32*/

/////////////////////////////////////////////////////////////////////////////

// This is a function on Windows so it can return ":" in a Unix
// emulation environment - such as Cygwin - and ";" otherwise.
#if defined(_WIN32)
PUTIL_CLASS CCS
PATHSEP(void)
{
    static CCS pathsep;

    if (!pathsep) {
	pathsep = ";";
	// This test was suggested for Cygwin but seems to have a good
	// hope of translating to other emulation environments. Of course
	// we could also define our own EV which would be dispositive.
	if (getenv("TERM")) {
	    pathsep = ":";
	}
    }
    return pathsep;
}
#endif	/*!_WIN32*/

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_exit_(CCS f, int l, int status)
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
    if (path_max <= 0)
	path_max = 256; /* should never happen but be safe if it does */
#endif

    return path_max;
}

/////////////////////////////////////////////////////////////////////////////

// This protoype is based on the Win32 SearchPath function for compatibility.
PUTIL_CLASS size_t
putil_searchpath(CCS srchpath, CCS prog, CCS ext,
		    unsigned long buflen, CS buf, CS *basepart)
{
#ifdef _WIN32
    return SearchPath(srchpath, prog, ext, buflen, buf, basepart);
#else	/*_WIN32*/
    CS spath, p, e;
    size_t len = 0;
    int baselen;

    ext = ext;	// to keep compiler happy - we don't use extensions on unix.
    
    baselen = strlen(prog) + 1;

    buf[0] = '\0';

    if (strchr(prog, '/')) {
	CS rbuf;

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
	if (! (p = getenv("PATH")))
	    return 0;
	spath = putil_strdup(p);
    }

    for (p = spath, e = strchr(p, ':'); p; p = e + 1) {

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
	if (basepart)
	    *basepart = endof(buf);
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
#endif	/*_WIN32*/
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_error_(CCS f, int l, CCS fmt, ...)
{
    _PUTIL_PRINTMSG_(Error, f,l, fmt);
    _PUTIL_ERROR_DEBUG_(2, f, l);
}

/////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
PUTIL_CLASS void
putil_errorW_(CCS f, int l, WCHAR *fmt, ...)
{
    _PUTIL_PRINTMSGW_(Error, f,l, fmt);
    _PUTIL_ERROR_DEBUG_(2, f, l);
}
#endif	/*_WIN32*/

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_int_(CCS f, int l, CCS fmt, ...)
{
    _PUTIL_PRINTMSG_(Internal Error, f, l, fmt);
#if !defined(_WIN32)
    unlink("/cores/core");	// OS X doesn't like to overwrite core files
#endif	/*!_WIN32*/
    abort();
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_die_(CCS f, int l, CCS fmt, ...)
{
    _PUTIL_PRINTMSG_(Error, f,l, fmt);
    _PUTIL_ERROR_DEBUG_(0, f, l);
    exit(2);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_warn_(CCS f, int l, CCS fmt, ...)
{
    _PUTIL_PRINTMSG_(Warning, f, l, fmt);
    _PUTIL_ERROR_DEBUG_(2, f, l);
}

/////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
PUTIL_CLASS void
putil_warnW_(CCS f, int l, WCHAR *fmt, ...)
{
    _PUTIL_PRINTMSGW_(Warning, f, l, fmt);
    _PUTIL_ERROR_DEBUG_(2, f, l);
}
#endif	/*_WIN32*/

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_srcdbg_(CCS f, int l, CCS fmt, ...)
{
    CS t;

    if ((t = getenv("PUTIL_SRCDBG")) && atoi(t)) {
	_PUTIL_PRINTMSG_(Warning, f, l, fmt);
	_PUTIL_ERROR_DEBUG_(2, f, l);
    }
}

/////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
PUTIL_CLASS void
putil_win32err_(CCS f, int l, int code, DWORD last, CCS string)
{
    LPTSTR msg;

    DWORD flgs = FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;

    if(!FormatMessage(flgs, NULL, last, 0, (LPTSTR)&msg, 0, NULL)) {
	msg = "unknown error";
    }

    if (code) {
	putil_error_(f, l, "%s: [%ld] %s", string, last, msg);
	exit(code);
    } else {
	putil_warn_(f, l, "%s: [%ld] %s", string, last, msg);
    }
}
#endif	/*_WIN32*/

/////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
PUTIL_CLASS void
putil_win32errW_(CCS f, int l, int code, DWORD last, LPCWSTR string)
{
    LPWSTR msg;

    DWORD flgs = FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;

    if(!FormatMessageW(flgs, NULL, last, 0, (LPWSTR)&msg, 0, NULL)) {
	msg = L"unknown error";
    }

    if (code) {
	putil_errorW_(f, l, L"%s: [%ld] %s", string, last, msg);
	exit(code);
    } else {
	putil_warnW_(f, l, L"%s: [%ld] %s", string, last, msg);
    }
}
#endif	/*_WIN32*/

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_syserr_(CCS f, int l, int code, CCS string)
{
#if defined(_WIN32)
    putil_win32err_(f, l, code, GetLastError(), string);
#else	/*_WIN32*/
    char *msg;
    
    msg = strerror(errno);

    if (code) {
	if (string) {
	    putil_error_(f,l, "%s: %s", string, msg);
	} else {
	    putil_error_(f,l, "%s", msg);
	}
	exit(code);
    } else {
	if (string) {
	    putil_warn_(f,l, "%s: %s", string, msg);
	} else {
	    putil_warn_(f,l, "%s", msg);
	}
    }
#endif	/*_WIN32*/
}

/////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
PUTIL_CLASS void
putil_syserrW_(CCS f, int l, int code, WCHAR *string)
{
    putil_win32errW_(f, l, code, GetLastError(), string);
}
#endif	/*_WIN32*/

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_lnkerr_(CCS f, int l, int code, CCS name1, CCS name2)
{
#if defined(_WIN32)
    DWORD flgs = FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;
    LPTSTR msg;

    if(!FormatMessage(flgs, NULL, GetLastError(), 0, (LPTSTR)&msg, 0, NULL)) {
	msg = "unknown error";
    }
#else	/*_WIN32*/
    char *msg;
    
    msg = strerror(errno);
#endif	/*_WIN32*/

    if (code) {
	putil_error_(f,l, "%s -> %s: %s", name2, name1, msg);
	exit(code);
    } else {
	putil_warn_(f,l, "%s -> %s: %s", name2, name1, msg);
    }
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS CS
putil_get_homedir(CS buf, size_t buflen)
{
    CS h;

    buf[0] = '\0';

    // If a $HOME EV exists, even on Windows, use it. This allows
    // support of Cygwin et al.
    if ((h = getenv("HOME"))) {
	if (bytelen(h) <= buflen) {
	    strcpy(buf, h);
	    return buf;
	}
    }

#ifdef _WIN32
    // Otherwise, on Windows, drop back to %HOMEDRIVE%%HOMEPATH%.
    if ((h = getenv("HOMEDRIVE"))) {
	strcpy(buf, h);
	if ((h = getenv("HOMEPATH"))) {
	    if (bytelen(buf) + bytelen(h) <= buflen) {
		strcat(buf, h);
		return buf;
	    } else {
		buf[0] = '\0';
	    }
	} else {
	    buf[0] = '\0';
	}
    }
#endif	/*_WIN32*/

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS CS
putil_get_systemdir(CS buf, size_t buflen)
{
#ifdef _WIN32
    CS h;

    buf[0] = '\0';
    if (!(h = getenv("SYSTEMROOT")))
	return NULL;
    _snprintf(buf, buflen, "%s", h);
#else	/*_WIN32*/
    snprintf(buf, buflen, "/etc");
#endif	/*_WIN32*/
    return buf;
}

/////////////////////////////////////////////////////////////////////////////

// Similar semantics to realpath() except that if the path does not
// exist it may optionally return a best guess of what its absolute path
// *should* look like instead of failing as realpath() does
// on some systems.
// Note that this will resolve symlinks, just as SUS says realpath() does.
// Also note that this is just as insecure as the original.
PUTIL_CLASS CS
putil_realpath(CCS path, int guess)
{
    CS resolved;
    size_t len;

    len = putil_path_max() + 1;
    resolved = (CS)putil_malloc(len);

#ifdef _WIN32
    {
	DWORD ret = GetFullPathName(path, len, resolved, NULL);
	if (ret == 0 || ret > len) {
	    putil_free(resolved);
	    return NULL;
	}
    }
#else	/*_WIN32*/
    if (realpath(path, resolved)) {
	// Feeble attempt at making realpath more secure ...
	if (strlen(resolved) >= len)
	    abort();
    } else {
	if (!guess) {
	    putil_free(resolved);
	    return NULL;
	}

	if (errno != ENOENT) {
	    putil_free(resolved);
	    return NULL;
	} else if (*path == '/') {
	    char *parent;

	    // This recursion should always break at "/" (the root).
	    if ((parent = putil_dirname(path))) {
		CS rbuf;

		if ((rbuf = putil_realpath(parent, guess))) {
		    snprintf(resolved, leftlen(resolved), "%s/%s", rbuf, basename((CS)path));
		    putil_free(parent);
		    putil_free(rbuf);
		} else {
		    putil_free(parent);
		    putil_free(resolved);
		    return NULL;
		}
	    }
	} else {
	    if (getcwd(resolved, len - charlen(path) - sizeof(char))) {
		strcat(resolved, "/");
		strcat(resolved, path);
	    } else {
		putil_free(resolved);
		return NULL;
	    }
	}
    }

    // Some realpaths (Solaris) return NULL if path doesn't exist,
    // others (FreeBSD) don't care.
    if (!guess && access(resolved, F_OK)) {
	putil_free(resolved);
	return NULL;
    }
#endif	/*_WIN32*/

    return resolved;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS CS
putil_basename(CCS path)
{
#ifdef _WIN32
    char fname[_MAX_FNAME], ext[_MAX_EXT], buf[_MAX_PATH];
    _splitpath(path, NULL, NULL, fname, ext);
    strcpy(buf, fname);
    strcat(buf, ext);
    if (!stricmp(endof(path) - strlen(buf), buf)) {
	return (CS)(endof(path) - strlen(buf));
    } else {
	return putil_strdup(buf);
    }
#else	/*_WIN32*/
    return basename((CS)path);
#endif	/*_WIN32*/
}

/////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
// Like readlink(2) but allocates and returns a null-terminated buffer.
PUTIL_CLASS CCS
putil_readlink(CCS path)
{
    ssize_t len, l1;
    CS buf;

    if (!path)
	return NULL;

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
#endif

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS CS
putil_dirname(CCS path)
{
    CS result;

    if (!path)
	return NULL;

    result = (CS)putil_malloc(strlen(path) + 2); /* may need extra \ on &*#@ Windows */

#ifdef _WIN32
    {
	char buf[PATH_MAX];
	CS ep;
	char drive[_MAX_DRIVE], dir[_MAX_DIR];

	strcpy(buf, path);
	for (ep = endof(buf) - 1; *ep == '\\' || *ep == '/'; ep--) {
	    *ep = '\0';
	}
	_splitpath(buf, drive, dir, NULL, NULL);
	result[0] = '\0';
	strcat(result, drive);
	if (*dir) {
	    strcat(result, dir);
	} else {
	    strcat(result, "\\");
	}
	for (ep = endof(result) - 1; *ep == '\\' || *ep == '/'; ep--)
		*ep = '\0';
    }
#else	/*_WIN32*/
    // We do this double copying because some dirname()
    // implementations (FreeBSD) return a ptr to static storage
    // while others (Solaris) modify the parameter string. The
    // following is safe in both cases, thread-safety aside.
    {
	char *buf;

	buf = putil_strdup(path);
	strcpy(result, dirname(buf));
	putil_free(buf);
    }
#endif	/*_WIN32*/
    return result;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS int
putil_mkdir_p(CCS path)
{
    char *parent;

    if ((parent = putil_dirname(path))) {
	if (access(parent, F_OK))
	    (void)putil_mkdir_p(parent);
	putil_free(parent);
    }
#ifdef	_WIN32
    return mkdir(path);
#else	/*_WIN32*/
    return mkdir(path, 0777);
#endif	/*_WIN32*/
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS CCS
putil_prog(void)
{
    static CCS me = NULL;

    if (me != NULL) {
	return me;
    }

#if defined(BSD)
    if ((me = getprogname())) {
	return me;
    } else {
	return "????";
    }
#else	/*BSD*/
    if ((me = putil_getexecpath())) {
	CCS lastf = strrchr(me, '/');
	CCS lastb = strrchr(me, '\\');
	if (lastf && lastb) {
	    me = (lastf > lastb) ? (lastf + 1) : (lastb + 1);
	} else if (lastf) {
	    me = lastf + 1;
	} else if (lastb) {
	    me = lastb + 1;
	}
	return me;
    } else {
	return "????";
    }
#endif	/*!BSD*/
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
PUTIL_CLASS CS *
putil_argv_from_environ(int *p_argc)
{
#ifdef	_WIN32
    return NULL;
#else	/*_WIN32*/

#if defined(__APPLE__)
    // OSX has its own way (as usual).
    CS *av;
    av = *_NSGetArgv();
    if (p_argc) {
	int i;
	for (i = 0; av[i]; i++)
	*p_argc = i;
    }
    return av;
#else	/*__APPLE__*/
    static char **orig_environ;
    CS *p;

    // In case we end up getting called late, make sure to remember the
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
	ULONG64 not_ptr = ARG_MAX/sizeof(CS);
	ULONG64 p2i;
	do {
	    p2i = (ULONG64) *--p;
	} while (p2i > not_ptr);
	if (p_argc)
	    *p_argc = (int)p2i;
	return p+1;
    } else {
	return NULL;
    }
#endif	/*__APPLE__*/
#endif	/*_WIN32*/
}

/////////////////////////////////////////////////////////////////////////////

// Special rule for putil_getexecpath: if a program is going to
// use it, it should be called asap before the 'environ' array
// has a chance to get realloc-ed away from argv. This will cause
// the result to be stashed in a static buffer, so it may safely
// be used later as long as it was called early.
PUTIL_CLASS CCS
putil_getexecpath(void)
{
    static char xname[PATH_MAX];
#ifdef _WIN32
    if (xname[0] == '\0')
	GetModuleFileName(NULL, xname, charlen(xname));
    return xname;
#else	/*_WIN32*/
    char buf[PATH_MAX + 1], pbuf[PATH_MAX];
    ssize_t llen;

    // This can't change once we derive it ...
    if (xname[0] != '\0')
	return xname;

    memset(buf, 0, sizeof(buf));

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
#if defined(linux) || defined(__CYGWIN__)
    if ((llen = readlink("/proc/self/exe", buf, charlen(buf) - 1)) == -1) {
	char **e;
	CS p;

	for (e = environ; *(e + 1); e++);
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
#elif defined(sun)
    {
	// This /proc entry is only available as of Solaris 10, but
	// if present it's the most reliable.
	if ((llen = readlink("/proc/self/path/a.out", buf, charlen(buf) - 1)) == -1) {
	    // One danger of getexecname() is that if you run "foo"
	    // which is a symlink to "bar", getexecname() returns "bar".
	    // Otherwise it's pretty strong with the caveat mentioned
	    // in the man page about relative paths and changing cwd.
	    (void)strcpy(buf, getexecname());
	    if (! putil_is_absolute(buf)) {
		char cwd[PATH_MAX];
		if (getcwd(cwd, charlen(cwd)) == NULL)
		    putil_syserr(1, "getcwd");
		strcat(cwd, "/");
		strcat(cwd, buf);
		strcpy(buf, cwd);
	    }
	} else {
	    buf[llen] = '\0';
	}
    }
#else

    {
#if defined(BSD)
	// As of FreeBSD 5.x the /proc filesystem isn't mounted by
	// default so we can't count on the following to work. If the
	// file is present it should be reliable; if not we fall back.
	if ((llen = readlink("/proc/curproc/file", buf, charlen(buf) - 1)) == -1) {
	    buf[0] = '\0';
	} else {
	    buf[llen] = '\0';
	}
#endif	/*BSD*/

	if (! putil_is_absolute(buf)) {
	    // At this point we've found we're on a system without a
	    // 100% ironclad way of getting the exec path. Try digging
	    // up the argv, which only works if argv immediately
	    // precedes the environ block in memory, and seeing if we
	    // can do something with argv[0]. Note that many Unix books,
	    // FAQs, etc. deprecate this but it does usually "work".
	    char **argv = putil_argv_from_environ(NULL);
	    if (argv) {
		(void)strcpy(buf, argv[0]);
	    } else {
		buf[0] = '\0';
		return NULL;
	    }
	}
    }

#endif

    // If we ended up with an absolute path we're done.
    if (putil_is_absolute(buf)) {
	return strcpy(xname, buf);
    }

    // Search PATH for it.
    if (putil_searchpath(NULL, buf, NULL, charlen(pbuf), pbuf, NULL))
	return strcpy(xname, pbuf);

    // Last try - look for it in the CWD.
    if (!access(buf, X_OK)) {
	if (getcwd(xname, charlen(xname)) == NULL)
	    putil_syserr(1, "getcwd");
	strcat(xname, "/");
	return strcat(xname, buf);
    }

    buf[0] = '\0';
    return NULL;
#endif /*_WIN32*/
}

PUTIL_CLASS CS
putil_canon_path(CS path, CS newpath, size_t len)
{
#ifdef _WIN32
    CS buf, p;

    if (newpath) {
	p = buf = strncpy(newpath, path, len);
    } else {
	p = buf = path;
    }

    for (; *p; p++) {
	if (*p == '\\')
	    *p = '/';
    }

    for (p = strchr(buf, '\0') - 1; *p == '/'; *p-- = '\0');

    return buf;
#else	/*_WIN32*/
    if (newpath) {
	return strncpy(newpath, path, len);
    } else {
	return path;
    }
#endif	/*_WIN32*/
}

/////////////////////////////////////////////////////////////////////////////

// There's no strstr() function on Windows so we synthesize one.
PUTIL_CLASS CS
putil_path_strstr(CCS s1, CCS s2)
{
#ifdef _WIN32
    size_t len;
    char c1, c2;

    for (len = strlen(s2); *s1; s1++) {
	c1 = *s1;
	c2 = *s2;
	if (_tolower(c1) == _tolower(c2))
	    if (!strnicmp(s1, s2, len))
		return (CS)s1;
    }
    return NULL;
#else	/*_WIN32*/
    return strstr(s1, s2);
#endif	/*_WIN32*/
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void *
putil_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr == NULL)
	putil_syserr(2, "malloc");
    return ptr;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void *
putil_malloc_str(size_t size)
{
    return putil_malloc(size * sizeof(char));
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void *
putil_calloc(size_t nelem, size_t elsize)
{
    void *ptr = calloc(nelem, elsize);
    if (ptr == NULL)
	putil_syserr(2, "calloc");
    return ptr;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void *
putil_realloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (ptr == NULL)
	putil_syserr(2, "realloc");
    return ptr;
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS void
putil_free_(const void *ptr)
{
    free((void *)ptr);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS CS
putil_strdup(CCS s1)
{
    size_t len = bytelen(s1);
    CS newstr = (CS)putil_malloc(len + sizeof(char));
    return strcpy(newstr, s1);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS CS
putil_getenv(CCS name)
{
    return getenv(name);
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS int
putil_putenv(CS string)
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
putil_unsetenv(CCS name)
{
#if defined(_WIN32)
    SetEnvironmentVariable(name, NULL);
#else	/*_WIN32*/
    CS *ep;
    size_t len;

    len = strlen(name);
    for (ep = environ; *ep; ++ep) {
	if (!strncmp(*ep, name, len) && ep[0][len] == '=') {
	    CS *np;

	    for (np = ep; np[0]; np++)
		np[0] = np[1];
	}
    }
#endif	/*_WIN32*/
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS int
putil_uname(struct utsname *name)
{
#if defined(_WIN32)
    OSVERSIONINFO osvi = {0};
    DWORD nodelen = sizeof(name->nodename);

    strcpy(name->sysname,  "Windows");
    strcpy(name->nodename, "");
    strcpy(name->release,  "");
    strcpy(name->version,  "");
    strcpy(name->machine,  "x86");

    if (!GetComputerNameEx(ComputerNameDnsHostname, name->nodename, &nodelen))
	return -1;

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!GetVersionEx(&osvi))
	return -1;
    _snprintf(name->release, sizeof(name->release), "%d.%d",
	osvi.dwMajorVersion, osvi.dwMinorVersion);
    return 0;
#else	/*_WIN32*/
    return uname(name);
#endif	/*_WIN32*/
}

/////////////////////////////////////////////////////////////////////////////

PUTIL_CLASS CS *
putil_prepend2argv(CS *argv, ...)
{
    char **nargv, **np;
    CS str;
    int i;
    va_list ap;

    // Count the strings in argv.
    for (i = 0; argv[i]; i++);

    // Count the additional strings.
    va_start(ap, argv);
    for (; va_arg(ap, CCS); i++);
    va_end(ap);

    // Allocate the new argv
    nargv = np = (char **)putil_malloc((i + 1) * sizeof(nargv[0]));

    // Rewind 'ap' and put the extras at the front of the array ...
    va_start(ap, argv);
    while ((str = (CS) va_arg(ap, CS))) {
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

#ifdef _WIN32
PUTIL_CLASS CCS
putil_tmpdir(CS tbuf, DWORD tlen)
{
    DWORD len = GetTempPath(tlen, tbuf);
    if (len == 0 || len > tlen)
	putil_die("unable to determine temp dir");
    return tbuf;
}
#else	/*_WIN32*/
PUTIL_CLASS CCS
putil_tmpdir(CS tbuf, size_t tlen)
{
    CCS t;

    if (!(t = getenv("TMPDIR")))
	t = "/tmp";
    snprintf(tbuf, tlen, "%s/", t);
    return tbuf;
}
#endif	/*_WIN32*/

// Simply to help suppressing warnings. The space is there because
// otherwise we get a warning from __attribute__(( __format__)).
#define PUTIL_NULLSTR		" "
#define PUTIL_NULLSTRW		L" "

// This exists only to suppress a bogus warning emitted by gcc when
// you define a static func but don't call it in the same
// compilation unit. This function calls itself recursively to try
// to avoid that same warning.
void
_putil_never_called(void)
{
    putil_builton();
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
    putil_canon_path(PUTIL_NULLSTR, PUTIL_NULLSTR, 0);
    putil_path_strstr(PUTIL_NULLSTR, PUTIL_NULLSTR);
    putil_malloc(0);
    putil_malloc_str(0);
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

#if defined(_WIN32)
    putil_errorW(PUTIL_NULLSTRW);
    putil_warnW(PUTIL_NULLSTRW);
    putil_win32err_(PUTIL_NULLSTR, 0, 0, 0, PUTIL_NULLSTR);
    putil_win32errW_(PUTIL_NULLSTR, 0, 0, 0, PUTIL_NULLSTRW);
    putil_syserrW_(PUTIL_NULLSTR, 0, 0, PUTIL_NULLSTRW);
#else	/*_WIN32*/
    putil_readlink(NULL);
#endif	/*_WIN32*/

    // Avoid warning via bogus recursive call.
    if (_putil_never_called == _putil_never_called) {
	_putil_never_called();
    }
}

#undef PUTIL_NULLSTR
#undef PUTIL_NULLSTRW

#endif	/* PUTIL_NO_BODY */

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#undef PUTIL_API
#undef PUTIL_CLASS
#undef PUTIL_NO_BODY

/// @endcond RSN

#endif	/* PUTIL_H */
