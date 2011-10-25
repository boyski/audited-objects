// Copyright (c) 2005-2010 David Boyce.  All rights reserved.

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

/// @file
/// @brief The part of the auditor library which is specific
/// to Unix. More precisely it is specific to ELF-based,
/// SVR4-derived or -inspired operating systems. If, for instance,
/// there was ever an AIX auditor it would presumably need yet
/// another source file.

// Must be above putil stuff.
#include "trio.h"

/// @cond static
// Make sure we do this before bringing in AO.h because that defaults
// to declaring these functions non-static.
#define PUTIL_DECLARE_FUNCTIONS_STATIC
#include "Putil/putil.h"
/// @endcond

#include "AO.h"

static int _ignore_path(const char *);
static void _thread_mutex_lock(void);
static void _thread_mutex_unlock(void);

// We go out of our way to explicitly include these headers here, even
// though they've probably already been included, because we're about
// to #define the things they declare and it's critical that they be
// read and their include-guards set before that happens.
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#if defined(sun)
#include <thread.h>
#include <sys/systeminfo.h>
#endif	/*sun*/

// Here we declare alternate names for some functions we'll be
// intercepting in order that we can still call them directly as
// needed within the auditor.
static FILE *(*fopen_real) (const char *, const char *);
static pid_t(*fork_real) (void);
static int (*open_real) (const char *, int, ...);
static int (*close_real) (int);
static int (*rename_real) (const char *, const char *);
static int (*link_real) (const char *, const char *);
static int (*symlink_real) (const char *, const char *);
static int (*unlink_real) (const char *);
static void (*_exit_real) (int);

static int (*pthread_mutex_lock_real) (pthread_mutex_t *);
static int (*pthread_mutex_unlock_real) (pthread_mutex_t *);
static pthread_t(*pthread_self_real) (void);

// These will be used to synchronize access to this library's
// static data where required. Most static data is initialized
// while still single threaded and synchronization is not
// required there, but there are a few places "post main"
// where we need it.
static pthread_mutex_t StaticDataAccessMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ExecFlushMutex = PTHREAD_MUTEX_INITIALIZER;

#if defined(__CYGWIN__)
/*
 * An un(der)documented fact about Cygwin is that LD_PRELOAD works as long as you
 * call out each interceptable function via CW_HOOK as shown. In order to avoid
 * dragging in <windows.h> here and confusing everyone, we provide the DllMain
 * signature in terms of basic types, presuming it can never change.
 * Another strange thing about Cygwin is that it does not implement RTLD_NEXT,
 * only RTLD_DEFAULT, and yet RTLD_DEFAULT seems to behave like RTLD_NEXT.
 * Not complaining, because it works for us. Just saying.
 */
#include <sys/cygwin.h>
int __attribute__((__stdcall__)) DllMain(void *hinstDLL, unsigned long fdwReason, void *lpvReserved)
{
    (void)hinstDLL; (void)fdwReason; (void)lpvReserved;
    cygwin_internal (CW_HOOK, "_Exit", _Exit);
    cygwin_internal (CW_HOOK, "_exit", _exit);
    cygwin_internal (CW_HOOK, "close", close);
    cygwin_internal (CW_HOOK, "creat", creat);
    cygwin_internal (CW_HOOK, "execv", execv);
    cygwin_internal (CW_HOOK, "execve", execve);
    cygwin_internal (CW_HOOK, "execvp", execvp);
    cygwin_internal (CW_HOOK, "fopen", fopen);
    cygwin_internal (CW_HOOK, "freopen", freopen);
    cygwin_internal (CW_HOOK, "open64", open64);
    cygwin_internal (CW_HOOK, "fork", fork);
    cygwin_internal (CW_HOOK, "link", link);
    cygwin_internal (CW_HOOK, "mkdir", mkdir);
    cygwin_internal (CW_HOOK, "open", open);
    cygwin_internal (CW_HOOK, "popen", popen);
    cygwin_internal (CW_HOOK, "pthread_create", pthread_create);
    cygwin_internal (CW_HOOK, "pthread_exit", pthread_exit);
    cygwin_internal (CW_HOOK, "rename", rename);
    cygwin_internal (CW_HOOK, "symlink", symlink);
    cygwin_internal (CW_HOOK, "system", system);
    cygwin_internal (CW_HOOK, "unlink", unlink);
    cygwin_internal (CW_HOOK, "vfork", vfork);
    return 1;
}
#endif

// Contains some declarations used by the common code.
#include "Interposer/interposer.h"

/// @cond static
// For ease of use, and for the benefit of code that's used
// both inside and outside the auditor, we #define the above names
// back to the original name. This allows us to say
// 'open(foo,O_RDONLY)' and know that we'll get the "real"
// version of open() from either the auditor or our own app space.
// Code that's exclusive to the auditor can feel free to use foo_real()
// directly.
// Remember to undo these just after including libcommon.c!
#define fopen		fopen_real
#define fork		fork_real
#define open		open_real
#define close		close_real
#define rename		rename_real
#define link(p1,p2)	link_real(p1,p2)
#define symlink(p1,p2)	symlink_real(p1,p2)
#define unlink(p1)	unlink_real(p1)
#define _exit		_exit_real
/// @endcond static

// This drags in a LOT of source code ...
#include "libcommon.c"

// The special symbols above must be #undef-ed here
// so we can intercept them under their real names.
#undef fopen
#undef fork
#undef open
#undef close
#undef rename
#undef link
#undef symlink
#undef unlink
#undef _exit

#include "Interposer/close.h"
#include "Interposer/exec.h"
#include "Interposer/exit.h"
#include "Interposer/fork.h"
#include "Interposer/link.h"
#include "Interposer/open.h"
#include "Interposer/threads.h"
#include "Interposer/libinterposer.h"

//WRAP(int, chdir, (const char *path), path)

#if defined(BSD)
#include <sys/mount.h>
#if !defined(__APPLE__)
#include <elf.h>
#endif /*APPLE*/
#else	/*BSD*/
#include <sys/statvfs.h>
#include <sys/vfs.h>
#endif	/*BSD*/

#if !defined(__APPLE__) && !defined(__hpux__) && !defined(__CYGWIN__)
#include <link.h>
#endif /*APPLE*/

// Request the address of the "real" (system-supplied-and-documented)
// function of the specified name.
static void *
_get_real(const char *fcn)
{
    void *fcnptr;

    fcnptr = interposer_get_real(fcn);
    // Better to die now if symbols not found than to segfault later.
    if (!fcnptr) {
	putil_die("no such symbol: %s()", fcn);
    }
    return fcnptr;
}

/// This function will be called at the <i>earliest possible
/// opportunity</i> once the audited command is fully initialized
/// and running. This generally means at the first interposed
/// function called after main(). We would love to figure out a
/// way to get earlier, e.g. to interpose on main() itself, but
/// for now this is the best we have and it seems to work fine.
/// @param[in] call     the name of the interposed function call
/*static*/ void
interposed_late_init(const char *call)
{
    const char *exe;
    unsigned long pid;

    // Must be done before putenv() is called as that could
    // realloc the 'environ' pointer away from argv.
    exe = putil_prog();

    // Certain libc functions must be called from within this library.
    // We need the real semantics; wrappers are apt to get into loops.
    // *INDENT-OFF*
    fopen_real	 = (FILE *(*)(const char *, const char *))_get_real("fopen");
    fork_real	 = (pid_t(*)(void))_get_real("fork");
    open_real	 = (int(*)(const char *, int, ...))_get_real("open");
    close_real	 = (int(*)(int))_get_real("close");
    rename_real	 = (int(*)(const char *, const char *))_get_real("rename");
    link_real	 = (int(*)(const char *, const char *))_get_real("link");
    symlink_real = (int(*)(const char *, const char *))_get_real("symlink");
    unlink_real	 = (int(*)(const char *))_get_real("unlink");
    _exit_real	 = (void(*)(int))_get_real("_exit");
    // *INDENT-ON*

    if ((pid = _init_auditlib(call, exe, interposer_get_cmdline()))) {
	// Put the pid of the *current* process into the env block as the
	// *parent* cmd id.
	prop_mod_ulong(P_PCMDID, pid, NULL);
	// Turn on the auditor and go.
	interposer_set_active(1);
    } else {
	interposer_set_active(0);
    }
}

// Internal service routine. There are certain files we truly don't
// care about and just want to ignore as early as possible.
static int
_ignore_path(const char *path)
{
    int rc = 0;
    const char *pend;

    pend = endof(path);

    if (!strncmp(path, "/proc/", 6) || !strncmp(path, "/xfn/", 5) ||
	!strncmp(path, "/dev/", 5) || !strncmp(path, "/devices/", 9)) {
	// Typical "special" Unix file-like items.
	rc = 1;
    } else if (strstr(path, "/.ccache/")) {
	// There's no good reason to use ccache with AO but try to
	// behave reasonably if it happens anyway.
	rc = 1;
#if defined(sun)
    } else if (!strncmp(path, "/tmp/workshop", 13)) {
	// The Sun Workshop (aka Forte etc) (5.0? 6.0) compiler apparently
	// creates and does not remove a file beginning like this.
	rc = 1;
#elif defined(linux) || defined(__CYGWIN__)
    } else if (!strcmp(path, "/etc/mtab") || !strcmp(path, "/proc/mounts")) {
	// On Linux all processes seem to open /etc/mtab.
	// And we ourselves need to open /proc/mounts.
	rc = 1;
#elif defined(__APPLE__)
    } else if (strstr(path, "-Tmp-")) {
	// There's a lot of screwy stuff going on under /private/var/...
	// in OS X 10.5. No idea what these files are but they certainly
	// appear to be temp files based on their names.
	rc = 1;
    } else if (!strncmp(path, "/cores/", 7)) {
	// Vide infra. This is where OS X puts its core files.
	rc = 1;
#endif	/*__APPLE__*/
    } else if ((pend - 5) >= path && !strcmp(pend - 5, "/core")) {
	// Nothing more embarrassing than dumping core (esp. if it's
	// our fault) and then uploading the core file ...
	// IDEA but it may be better to skip these only at upload time
	// (i.e. record the action but don't save the result).
	// There are legitimate operations which *read* a core file.
	rc = 1;
    } else if (util_is_tmp(path)) {
	// We used to not trust this, on the theory that data is often
	// communicated between processes in the form of temp files,
	// such as a compiler leaving a .s file in /tmp for the
	// assembler to process and remove. But it was enabled and
	// the test suite still passes, so left in at least until
	// it causes trouble. If we stop filtering tmp files here we
	// need to start filtering them at upload time.
	rc = 1;
    } else if (!strncmp(path, "/tmp/sh", 7) && isdigit((int)path[7])) {
	// The shell tends to keep here-documents in a file called
	// "/tmp/shnnnnn" where "nnnnn" is based on the pid.
	rc = 1;
    } else if (IgnorePathRE && re_match__(IgnorePathRE, path)) {
	// User-configurable set of ignorable files. We may
	// eventually want to shift the hard-wired patterns
	// above to this model.
	rc = 1;
    }

    return rc;
}

static void
_thread_mutex_lock(void)
{
    if (pthread_mutex_lock_real) {
	if (pthread_mutex_lock_real(&StaticDataAccessMutex)) {
	    putil_syserr(2, "pthread_mutex_lock");
	}
    }
}

static void
_thread_mutex_unlock(void)
{
    if (pthread_mutex_unlock_real) {
	(void)pthread_mutex_unlock_real(&StaticDataAccessMutex);
    }
}

// Note that many programs walk through PATH calling exec on
// each possible executable path. Thus this may be called many
// times in succession and fail each time but the last.
static int
_pre_exec(const char *call, const char *path,
	  char *const *argvp[], char *const envp[])
{
    int rc = 0;

    // We used to need these here, and may again.
    UNUSED(argvp);
    UNUSED(envp);

    // Don't waste time sending the audit unless it looks like
    // the exec isn't going to return.
    if (!path || access(path, X_OK)) {
	return rc;
    }

    // OK, it looks like an exec is likely to find an executable file,
    // so we're about to embark on a suicide mission. Flush and
    // send all accumulated data in case we don't come back,
    // but leave the audit open in the unlikely event we do.
    // Note: we need to synchronize around this flush because it
    // operates on AuditFD from a potentially multithreaded code path.

    if (pthread_mutex_lock_real) {
	if (pthread_mutex_lock_real(&ExecFlushMutex)) {
	    putil_syserr(2, "pthread_mutex_lock");
	}
    }

    _audit_end(call, NOT_EXITING, 0);

    if (pthread_mutex_unlock_real) {
	(void)pthread_mutex_unlock_real(&ExecFlushMutex);
    }

    return rc;
}

/// Interposes over the execv() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     the pathname of the executable file
/// @param[in] argv     an argument vector for the new command
/// @return same as the wrapped function
/*static*/ int
execv_wrapper(const char *call,
	      int (*next) (const char *, char *const argv[]),
	      const char *path, char *const argv[])
{
    int ret, dbg;
    WRAPPER_DEBUG("ENTERING execv_wrapper() => %p [%s ...]\n", next, path);

    dbg = _pre_exec(call, path, &argv, NULL);

    if (dbg) {
	ret = (*next)(argv[0], argv);
    } else {
	ret = (*next)(path, argv);
    }

    return ret;
}

/// Interposes over the execve() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     the pathname of the executable file
/// @param[in] argv     an argument vector for the new command
/// @param[in] envp     an environment vector for the new command
/// @return same as the wrapped function
/*static*/ int
execve_wrapper(const char *call,
	       int (*next) (const char *, char *const argv[],
			    char *const envp[]), const char *path,
	       char *const argv[], char *const envp[])
{
    int ret, dbg;
    WRAPPER_DEBUG("ENTERING execve_wrapper() => %p [%s ...]\n", next, argv[0]);

    dbg = _pre_exec(call, path, &argv, envp);

    if (!envp) {
	// This is actually undefined behavior so we might as well
	// let the OS handle it. It's conceivable that some OS might
	// treat a null envp as an empty environment or like a
	// regular execv() so we don't throw our own error.
	if (dbg) {
	    ret = (*next)(argv[0], argv, envp);
	} else {
	    ret = (*next)(path, argv, envp);
	}
    } else {
	size_t plen;
	char **pblock, **ep;
	int found;

	// If the audited program uses exec[lv]e to compose
	// its own environment, we need special-case code to force our
	// own control EV's back into that env block.
	// The code below will return a new env block with our
	// properties re-exported to it. The envp starts at (pblock+1).

	plen = prop_new_env_block_sizeA(envp);

	pblock = (char **)alloca(plen);
	memset(pblock, 0, plen);

	(void)prop_custom_envA(pblock, envp);

	// If the host process assembles a custom environment which
	// does not retain LD_PRELOAD, that process branch will be
	// unaudited. This is a hole we ought to plug but it seems
	// unlikely so for now we will just warn about it. If it
	// ever actually happens it should be easy enough to fix.
	for (found = 0, ep = pblock + 1; *ep; ep++) {
	    if (!strncmp(*ep, PRELOAD_EV, strlen(PRELOAD_EV))) {
		found = 1;
		break;
	    }
	}
	if (!found) {
	    putil_warn("%s setting lost in exev[lv]e() call", PRELOAD_EV);
	}

	if (dbg) {
	    ret = (*next)(argv[0], argv, pblock + 1);
	} else {
	    ret = (*next)(path, argv, pblock + 1);
	}
    }

    return ret;
}

#if defined(sun)
/*static*/ int
isaexec_wrapper(const char *call,
		int (*next) (const char *, char *const argv[],
			     char *const envp[]), const char *path,
		char *const argv[], char *const envp[])
{
    int ret, dbg = 0;
    long sysret;
    size_t len;
    long isalen = 255;
    char *isalist, *pathbuf, *p, *isa, *lasts;
    const char *name;

    isalist = putil_malloc(isalen);
    while ((sysret = sysinfo(SI_ISALIST, isalist, isalen) > isalen)) {
	isalen = sysret;
	isalist = putil_realloc(isalist, isalen);
    }

    if (sysret == -1) {
	return -1;
    }

    len = strlen(path) + isalen;
    pathbuf = putil_malloc(len);
    strcpy(pathbuf, path);
    if ((p = strrchr(pathbuf, '/'))) {
	*++p = '\0';
	name = path + (p - pathbuf);
    } else {
	name = path;
	pathbuf[0] = '\0';
    }
    len = strlen(pathbuf);

    // We could handle the isaexec wrapper multiple ways: e.g. doing
    // the entire logic here and never calling through to 'next' by
    // making a call to either execve() or execve_wrapper(). The
    // current strategy is to simply use this logic to predict what
    // path the 'real' isaexec() will find and flush based on it.
    // This means that when we call through to 'next' it will make
    // an inter-library call to execve() which we will *miss*, but
    // presumably the work that would have been done in execve's
    // wrapper will have already been done here.

    for (isa = strtok_r(isalist, " ", &lasts); isa;
	 isa = strtok_r(NULL, " ", &lasts)) {
	strcpy(pathbuf + len, isa);
	strcat(pathbuf + len, "/");
	strcat(pathbuf + len + 1, name);
	if (!access(pathbuf, X_OK)) {
	    dbg = _pre_exec(call, pathbuf, &argv, envp);
	    break;
	}
    }

    putil_free(pathbuf);
    putil_free(isalist);

    if (dbg) {
	ret = (*next)(argv[0], argv, envp);
    } else {
	ret = (*next)(path, argv, envp);
    }

    return ret;
}
#endif				/*sun */

/// Interposes over the execvp() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] file     the name of the executable file
/// @param[in] argv     an argument vector for the new command
/// @return same as the wrapped function
/*static*/ int
execvp_wrapper(const char *call,
	       int (*next) (const char *, char *const argv[]),
	       const char *file, char *const argv[])
{
    int ret, dbg;
    char buf[PATH_MAX];
    WRAPPER_DEBUG("ENTERING execvp_wrapper() => %p [%s ...]\n", next, file);

    if (strchr(file, '/')) {
	dbg = _pre_exec(call, file, &argv, NULL);
    } else if (putil_searchpath(NULL, file, NULL, sizeof(buf), buf, NULL)) {
	dbg = _pre_exec(call, buf, &argv, NULL);
    } else {
	dbg = _pre_exec(call, NULL, &argv, NULL);
    }

    if (dbg) {
	ret = (*next)(argv[0], argv);
    } else {
	ret = (*next)(file, argv);
    }

    return ret;
}

/***************************************************************
 * There's no need for execl_wrapper(), execle_wrapper(), or
 * execlp_wrapper(); they are redirected automatically to the
 * corresponding execv*_wrapper() functions.
 **************************************************************/

/// Interposes over the fork() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @return same as the wrapped function
/*static*/ pid_t
fork_wrapper(const char *call, pid_t(*next) (void))
{
    pid_t pid;
    WRAPPER_DEBUG("ENTERING fork_wrapper() => %p [%s]\n", next, interposer_get_cmdline());

    // A subtlety: try the following command on Solaris:
    //   % truss -texec,creat,fork -f -a /bin/sh -c "date > foo"
    // This forks, then opens "foo" *on the child side of the fork*,
    // then execs "date" (also on the child side of course).
    // Interestingly, some shells (/usr/xpg4/bin/sh on Solaris) detect
    // that no fork is needed here and open "foo" in the original process.
    // NOTE: vfork calls come through here too for reasons explained
    // in its wrapper.

    // Flush all accumulated file audits at fork. This must be done
    // to ensure correct ordering of file mods. Consider the short
    // shell script "echo dada > foo; mv foo bar". The echo is
    // generally a shell builtin so it will be audited in the parent
    // (shell) process. The rename happens in a child (mv) process.
    // If audits are flushed at process end, the rename will show
    // up before the create which makes no sense. Also, the create
    // will be ignored since (as far as the shell knows) "foo" no
    // longer exists. There's no way for the shell to know about the
    // rename. But if we print audits prior to forks, foo will still
    // exist at print time and its creat record will precede the rename.
    _audit_flush(call);

    pid = (*next)();

    // Child side.
    if (pid == 0 && _auditor_isActive()) {
	// Open a new audit record for any files which might
	// be opened on the child side (prior to exec if any).
	// An alternative might be a lock on the existing file to
	// prevent concurrent writes.
	// Note: According to POSIX, in the instant after a fork
	// there should be only a single thread in the child so
	// the write to this static datum ought to be thread safe.
	AuditFD = _audit_open();

	// Mark this copy of the CA as not "started" because
	// no SOA has been, or will be, sent for this pid.
	// Note that we leave pid and ppid alone. This is a bit
	// of a lie of course but nobody really cares about the
	// actual pid of the process. We really only want the pid
	// to associate PAs with the CA they "belong" to. Files
	// touched by a child process still belong to the same
	// CA as the parent until an exec takes place in which
	// case they get a new one.
	ca_set_started(CurrentCA, 0);
    }

    return pid;
}

#if defined(sun)
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @return same as the wrapped function
/*static*/ pid_t
fork1_wrapper(const char *call, pid_t(*next) (void))
{
    return fork_wrapper(call, next);
}
#endif				/*sun */

/// Interposes over the vfork() function.
/// It's nearly impossible to wrap vfork() since the docs say:
/// "The procedure that called vfork() should not
/// return while running in the child's context ...". Any
/// wrapper function would have to return to reach the exec().
/// So we convert vfork to fork. There may be a few corner
/// cases where this affects behavior but generally use of
/// vfork is an anachronism, and SUS does contain verbiage to
/// the effect that vfork may be equivalent to fork.
/// Better ideas solicited. In particular it appears that
/// we could hack in some assembler code to play with the
/// stack. See a response by Jonathan Adams in comp.unix.solaris
/// from 2004-09-17, subject 'how to "wrap" vfork?'
/// @return same as the wrapped function
/*static*/ pid_t
vfork(void)
{
    const char *call = "vfork";
    interposer_late_init(call);
    WRAPPER_DEBUG("ENTERING vfork_wrapper() => %p\n", fork_real);

    if (interposer_get_active()) {
	return fork_wrapper(call, fork_real);
    } else {
	return fork_real();
    }
}

/// Interposes over the system() function.
/// We intercept system() because it contains an implicit fork which
/// requires an audit flush.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] str      the shell command
/// @return same as the wrapped function
/*static*/ int
system_wrapper(const char *call, int (*next) (const char *), const char *str)
{
    int ret;
    WRAPPER_DEBUG("ENTERING system_wrapper() => %p [%s ...]\n", next, str);

    // See comments in fork() wrapper.
    _audit_flush(call);

    ret = (*next)(str);

    return ret;
}

/// Interposes over the popen() function.
/// We intercept popen() because it contains an implicit fork which
/// requires an audit flush.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] command  the shell command
/// @param[in] mode     the mode in which the pipe is opened
/// @return same as the wrapped function
/*static*/ FILE *
popen_wrapper(const char *call,
	      FILE *(*next)(const char *, const char *),
	      const char *command, const char *mode)
{
    FILE *ret;
    WRAPPER_DEBUG("ENTERING popen_wrapper() => %p [%s ...]\n", next, command);

    // See comments in fork() wrapper.
    _audit_flush(call);

    ret = (*next)(command, mode);

    return ret;
}

// We probably should have an interposer for posix_spawn here ...

static void
_enable_thread_support(const char *call)
{
    UNUSED(call);
    //fprintf(stderr, "NEW THREAD!\n");

    // Thread interfaces may be in libc.so or in a different library
    // such as libpthread.so, and thus they are not automatically
    // available to all programs. We want to handle threading
    // (synchronize access to static data, track thread creation,
    // report thread ids in threaded programs, etc) without
    // opening Pandora's box for single-threaded programs. Thus
    // we initialize these pointers only if we got here, a clear
    // indication of a threaded program. They'll be available
    // for subsequent use iff they're non-null.
    if (!pthread_mutex_lock_real) {
	// We can be pretty sure this will only be executed once,
	// just prior to the first thread creation (which will actually
	// create thread #2 since the main thread is always #1).
	pthread_mutex_lock_real = _get_real("pthread_mutex_lock");
	pthread_mutex_unlock_real = _get_real("pthread_mutex_unlock");
	pthread_self_real = _get_real("pthread_self");
    }
}

/// Interposes over the pthread_create() function. Used to initialize
/// certain thread interfaces, on the theory that we won't need them
/// until we have multiple threads and we won't have multiple threads
/// until pthread_create() is called for the first time.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[out] thread  same as the wrapped function
/// @param[in] attr     same as the wrapped function
/// @param[in] start_func  same as the wrapped function
/// @param[in] arg      same as the wrapped function
/// @return same as the wrapped function
int
pthread_create_wrapper(const char *call,
		       int (*next) (pthread_t *, const pthread_attr_t *,
				    void *(*s) (void *), void *),
		       pthread_t * thread, const pthread_attr_t * attr,
		       void *(*start_func) (void *), void *arg)
{
    int ret;

    // IDEA Maybe we can generate a sufficiently unique thread sig
    // by combining the addresses of all 4 arguments, plus the value
    // of "__builtin_return_address(1)" and use that as the value in
    // a hash table where the keys are the official thread ids.
    // The official thread ids are transient but the sig would not be.

    _enable_thread_support(call);

    ret = (*next)(thread, attr, start_func, arg);

    return ret;
}

/// Interposes over the pthread_exit() function. This is not currently
/// of any use but may be needed if we can figure out how to do
/// auditing at thread granularity, which would require some kind
/// of deterministic thread id.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[out] value_ptr same as the wrapped function
/// @return same as the wrapped function
void
pthread_exit_wrapper(const char *call, void (*next) (void *), void *value_ptr)
{
    UNUSED(call);
    (*next)(value_ptr);
}

#if defined(sun)
/// Interposes over the thr_creat() function. Like pthread_create_wrapper()
/// but for Solaris thread compatibility. Sun's JVM uses these threads.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] stack_base same as the wrapped function
/// @param[in] stack_size same as the wrapped function
/// @param[in] start_func  same as the wrapped function
/// @param[in] arg      same as the wrapped function
/// @param[in] flags    same as the wrapped function
/// @param[out] new_ID  same as the wrapped function
/// @return same as the wrapped function
int
thr_create_wrapper(const char *call,
		   int (*next) (void *, size_t, void *(*s) (void *), void *,
				long, thread_t *), void *stack_base,
		   size_t stack_size, void *(*start_func) (void *), void *arg,
		   long flags, thread_t * new_ID)
{
    int ret;

    _enable_thread_support(call);

    ret = (*next)(stack_base, stack_size, start_func, arg, flags, new_ID);

    return ret;
}

/// Interposes over the thr_exit() function. See pthread_exit_wrapper().
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[out] status  same as the wrapped function
/// @return same as the wrapped function
void
thr_exit_wrapper(const char *call, void (*next) (void *), void *status)
{
    UNUSED(call);
    (*next)(status);
}
#endif				/*sun */

#if defined(linux) || defined(__CYGWIN__)
// I have no idea if this works but it should at least print a message
// so we know we've found an actual use of clone, at which time more
// work can be put into making it work right. To date I've never observed
// a direct use of this API.
// NOTE: this may be turned off right now - see <Interposer/fork.h>.
int
clone_wrapper(const char *call,
	      int (*next) (int (*)(void *), void *, int, void *),
	      int (*fn) (void *), void *child_stack, int flags, void *arg)
{
    int pid;
    WRAPPER_DEBUG("ENTERING %s_wrapper() => %p\n", call, next);

    // See comments in fork() wrapper.
    _audit_flush("clone");

    fprintf(stderr, "= about to %s(%ld)\n", call, (long)getpid());
    pid = (*next)(fn, child_stack, flags, arg);
    return pid;
}
#endif				/*linux */

/// Interposes over the open() function.
/// Note: here, 'next' may refer to either open() or open64().
/// Since they have the same signature they can share a wrapper.
/// In fact all of open/open64/creat/creat64 come through here.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] oflag    same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
open_wrapper(const char *call,
	     int (*next) (const char *, int, ...),
	     const char *path, int oflag, mode_t mode)
{
    int ret, saved_errno;
    WRAPPER_DEBUG("ENTERING open_wrapper() => %p [%s ...]\n", next, path);

    ret = (*next)(path, oflag, mode);
    saved_errno = errno;

    if (ret != -1) {
	op_e op;

	if (oflag & (O_RDWR | O_APPEND)) {
	    op = OP_APPEND;
	} else if (oflag & O_WRONLY) {
	    op = OP_CREAT;
	} else {
	    op = OP_READ;
	}

	_pa_record(call, path, NULL, ret, op);
    }

    errno = saved_errno;
    return ret;
}

/// Interposes over the open64() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] oflag    same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
open64_wrapper(const char *call,
	       int (*next) (const char *, int, ...),
	       const char *path, int oflag, mode_t mode)
{
    WRAPPER_DEBUG("ENTERING open64_wrapper() => %p [%s ...]\n", next, path);
    oflag |= O_LARGEFILE;
    return open_wrapper(call, next, path, oflag, mode);
}

#if defined(AT_FDCWD)

/// Interposes over the openat() function. This is currently a
/// Solaris-only API but other platforms may imitate it someday.
/// Note: here, 'next' may refer to either openat() or openat64().
/// Since they have the same signature they can share a wrapper.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] fildes   same as the wrapped function
/// @param[in] path     same as the wrapped function
/// @param[in] oflag    same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
openat_wrapper(const char *call,
	     int (*next) (int, const char *, int, ...),
	     int fildes, const char *path, int oflag, mode_t mode)
{
    int ret, saved_errno;

    ret = (*next)(fildes, path, oflag, mode);
    saved_errno = errno;

    if (ret != -1) {
	op_e op;

	if (oflag & (O_RDWR | O_APPEND)) {
	    op = OP_APPEND;
	} else if (oflag & O_WRONLY) {
	    op = OP_CREAT;
	} else {
	    op = OP_READ;
	}

	if (fildes == (int)AT_FDCWD || putil_is_absolute(path)) {
	    _pa_record(call, path, NULL, ret, op);
	} else {
	    char procbuf[PATH_MAX], linkbuf[PATH_MAX], atpath[PATH_MAX];

	    snprintf(procbuf, sizeof(procbuf), "/proc/%ld/path/%d",
		(long)getpid(), fildes);

	    memset(linkbuf, '\0', sizeof(linkbuf));
	    if (readlink(procbuf, linkbuf, sizeof(linkbuf)) > 0) {
		snprintf(atpath, sizeof(atpath), "%s/%s", linkbuf, path);
		_pa_record(call, atpath, NULL, ret, op);
	    } else {
		putil_warn("can't resolve %s relative to fd=%d", path, fildes);
	    }
	}
    }

    errno = saved_errno;
    return ret;
}

/// Interposes over the openat64() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] fildes   same as the wrapped function
/// @param[in] path     same as the wrapped function
/// @param[in] oflag    same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
openat64_wrapper(const char *call,
	       int (*next) (int, const char *, int, ...),
	       int fildes, const char *path, int oflag, mode_t mode)
{
    return openat_wrapper(call, next, fildes, path, oflag, mode);
}

#endif	/*AT_FDCWD*/

/// Interposes over the creat() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
creat_wrapper(const char *call,
	      int (*next) (const char *, mode_t), const char *path, mode_t mode)
{
    int oflag;
    WRAPPER_DEBUG("ENTERING creat_wrapper() => %p [%s ...]\n", next, path);

    UNUSED(next);
    oflag = O_WRONLY | O_CREAT | O_TRUNC;
    return open_wrapper(call, open_real, path, oflag, mode);
}

/// Interposes over the creat64() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
creat64_wrapper(const char *call,
		int (*next) (const char *, mode_t),
		const char *path, mode_t mode)
{
    int oflag;
    static int (*open64_real) (const char *, int, ...);
    WRAPPER_DEBUG("ENTERING creat64_wrapper() => %p [%s ...]\n", next, path);

    if (!open64_real) {
	open64_real = (int (*)(const char *, int, ...))_get_real("open64");
    }

    UNUSED(next);
    oflag = O_WRONLY | O_CREAT | O_TRUNC;
    return open64_wrapper(call, open64_real, path, oflag, mode);
}

static op_e
_mode2op(const char *mode)
{
    if (*mode == 'w' || (*mode == 'r' && strchr(mode, '+'))) {
	// Any previous data in the file is gone (w) or at risk (r+)
	return OP_CREAT;
    } else if (*mode == 'a' || strchr(mode, '+')) {
	// Write to end of file; previous data is safe.
	return OP_APPEND;
    } else {
	// Read access only; all data is safe.
	return OP_READ;
    }
}

/// Interposes over the fopen() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ FILE *
fopen_wrapper(const char *call,
	      FILE *(*next)(const char *, const char *),
	      const char *path, const char *mode)
{
    FILE *ret;
    int saved_errno;
    WRAPPER_DEBUG("ENTERING fopen_wrapper() => %p [%s ...]\n", next, path);

    ret = (*next)(path, mode);
    saved_errno = errno;

    if (ret) {
	_pa_record(call, path, NULL, fileno(ret), _mode2op(mode));
    }

    errno = saved_errno;
    return ret;
}

/// Interposes over the fopen64() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ FILE *
fopen64_wrapper(const char *call,
		FILE *(*next)(const char *, const char *),
		const char *path, const char *mode)
{
    WRAPPER_DEBUG("ENTERING fopen64_wrapper() => %p [%s ...]\n", next, path);
    return fopen_wrapper(call, next, path, mode);
}

/// Interposes over the freopen() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @param[in] stream   same as the wrapped function
/// @return same as the wrapped function
/*static*/ FILE *
freopen_wrapper(const char *call,
		FILE *(*next)(const char *, const char *, FILE *),
		const char *path, const char *mode, FILE *stream)
{
    FILE *ret;
    int saved_errno;
    WRAPPER_DEBUG("ENTERING freopen_wrapper() => %p [%s ...]\n", next, path);

    ret = (*next)(path, mode, stream);
    saved_errno = errno;

    if (ret) {
	_pa_record(call, path, NULL, fileno(ret), _mode2op(mode));
    }

    errno = saved_errno;
    return ret;
}

/// Interposes over the freopen64() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @param[in] stream   same as the wrapped function
/// @return same as the wrapped function
/*static*/ FILE *
freopen64_wrapper(const char *call,
		  FILE *(*next)(const char *, const char *, FILE *),
		  const char *path, const char *mode, FILE *stream)
{
    WRAPPER_DEBUG("ENTERING freopen64_wrapper() => %p [%s ...]\n", next, path);
    return freopen_wrapper(call, next, path, mode, stream);
}

/// Interposes over the close() function. We don't actually need
/// to track closes but we do need to make sure the host program
/// doesn't inadvertently close the audit descriptor.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] fildes   same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
close_wrapper(const char *call, int (*next) (int), int fildes)
{
    int ret;

    UNUSED(call);

    // For reasons of their own, some programs like to close all file
    // descriptors before an exec. This causes a problem for our
    // "private" audit descriptor so we protect it.
    if (fildes == AuditFD) {
	ret = 0;
    } else {
	ret = (*next)(fildes);
    }

    return ret;
}

/// Interposes over the mkdir() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
mkdir_wrapper(const char *call,
	     int (*next) (const char *, mode_t),
	     const char *path, mode_t mode)
{
    int ret;
    WRAPPER_DEBUG("ENTERING mkdir_wrapper() => %p [%s ...]\n", next, path);

    ret = (*next)(path, mode);
    if (ret == 0) {
	_pa_record(call, path, NULL, ret, OP_MKDIR);
    }

    return ret;
}

/// Interposes over the mkdirp() function, if present.
/// Note a couple of special complications here: first, we do
/// not explicitly record the creation of any directories but
/// the last right now. Such logic could be added but the hope
/// is that the last dir can serve as a proxy for the rest.
/// Once way to do this would be, on success, to stat each
/// our way back the dir path and assume it was just created
/// if the timestamp delta is less than 100 milliseconds or so.
/// Or, of course, assuming mkdirp will generally succeed, walk
/// along the path prior to the call using access().
/// Second, it's quite possible for mkdirp() to rely on mkdir(),
/// which emphasizes that our auditing strategy is to watch
/// the libc (or more properly the a.out/system lib) boundary
/// exclusively and completely ignore inter-library calls.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @param[in] mode     same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
mkdirp_wrapper(const char *call,
	     int (*next) (const char *, mode_t),
	     const char *path, mode_t mode)
{
    int ret;
    WRAPPER_DEBUG("ENTERING mkdirp_wrapper() => %p [%s ...]\n", next, path);

    ret = (*next)(path, mode);
    if (ret == 0) {
	_pa_record(call, path, NULL, ret, OP_MKDIR);
    }

    return ret;
}

/// Interposes over the link() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] existing same as the wrapped function
/// @param[in] newpath  same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
link_wrapper(const char *call,
	     int (*next) (const char *, const char *),
	     const char *existing, const char *newpath)
{
    int ret;
    WRAPPER_DEBUG("ENTERING link_wrapper() => %p [%s => %s]\n", next, existing, newpath);

    ret = (*next)(existing, newpath);
    if (ret == 0) {
	_pa_record(call, newpath, existing, -1, OP_LINK);
    }

    return ret;
}

/// Interposes over the symlink() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] name1    same as the wrapped function
/// @param[in] name2    same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
symlink_wrapper(const char *call,
		int (*next) (const char *, const char *),
		const char *name1, const char *name2)
{
    int ret;
    WRAPPER_DEBUG("ENTERING symlink_wrapper() => %p [%s -> %s]\n", next, name1, name2);

    ret = (*next)(name1, name2);
    if (ret == 0) {
	_pa_record(call, name2, name1, -1, OP_SYMLINK);
    }

    return ret;
}

/// Interposes over the rename() function.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] oldpath  same as the wrapped function
/// @param[in] newpath  same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
rename_wrapper(const char *call,
	       int (*next) (const char *, const char *),
	       const char *oldpath, const char *newpath)
{
    int ret;
    WRAPPER_DEBUG("ENTERING rename_wrapper() => %p [%s >> %s]\n", next, oldpath, newpath);

    ret = (*next)(oldpath, newpath);

    // Renames get VERY complicated. Sometimes a file is created by
    // one cmd and renamed in another, and we can't be sure in
    // what order the audit records will arrive. Current thinking
    // is that it's simplest to report a rename as an unlink followed
    // instantly by a create.  This may seem backward since rename
    // was invented to provide the atomicity that unlink+create
    // can't, but atomicity doesn't matter to us at recording time
    // since by now the operation has already succeeded.
    // Simplicity of aggregation is the priority.

    if (ret == 0) {
	struct stat64 ostbuf, nstbuf;

	// SUS says "rename(foo, foo)" is a no-op so we ignore that case.
	// Should be quite rare so not a concern from a performance POV.
	if (!stat64(oldpath, &ostbuf) && !stat64(newpath, &nstbuf)) {
	    if (ostbuf.st_ino == nstbuf.st_ino &&
		ostbuf.st_dev == nstbuf.st_dev) {
		return ret;
	    }
	}

	_pa_record(call, oldpath, NULL, -1, OP_UNLINK);
	_pa_record(call, newpath, NULL, -1, OP_CREAT);
    }

    return ret;
}

/// Interposes over the unlink() function.
/// We try to record all unlinks. Say a build script does a
/// "rm -f *.o core". During an audited build these files may or may
/// not exist so the unlink may fail, but on a subsequent recycled
/// build they may exist and we would want to get rid of them.
/// NOTE: it appears that GNU rm always issues an unlink() for
/// each argument whereas some other versions only unlink the file
/// if it appears to exist. Thus we find a lot more unlinks on Linux.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] path     same as the wrapped function
/// @return same as the wrapped function
/*static*/ int
unlink_wrapper(const char *call, int (*next) (const char *), const char *path)
{
    int ret;
    WRAPPER_DEBUG("ENTERING unlink_wrapper() => %p [%s]\n", next, path);

    // In this special case we record the op BEFORE performing it
    // in order to collect metadata from the path before it goes away.
    // Also, an unlink on a null string has been known to confuse the
    // audit processing so we ignore them since they're no-ops anyway.

    if (path && *path) {
	_pa_record(call, path, NULL, -1, OP_UNLINK);
    }

    ret = (*next)(path);

    return ret;
}

////////////////////////////////////////////////////////////////////
// See the comments on exit processing in <Interposer/exit.h>
////////////////////////////////////////////////////////////////////

/// This finalizer is called automatically as the auditor library
/// is being unmapped, but only if interception is on (aka active)
/// and only on "normal" exit as described in Interposer/exit.h.
/*static*/ void
interposed_finalize(void)
{
    if (_auditor_isActiveByRequest()) {
	pid_t pid;

	// This is a really cute trick I got from a guy named Nate Eldredge.
	// The exit status is not given to the library finalizer but we
	// need it, so what can we do? This is the answer: do a meaningless
	// fork. The child just returns so that the parent can wait for
	// it and gets its exit status. There's a slight chance that
	// the extra fork will cause non-beneficial side effects but
	// that should be quite rare.
	// IDEA Would it be better to use vfork() here? A big IDE
	// like Eclipse might have a large memory footprint and forking
	// it only to exit might be costly and/or have an OOM failure.
	// Answer, no, vfork is not allowed to return. There could be
	// an answer within posix_spawn but I haven't looked into it.
	if ((pid = fork_real()) == -1) {
	    putil_syserr(0, "fork");
	} else if (pid == 0) {
	    // The child just returns so the parent can wait for it.
	} else {
	    int wstat, status = 7;

	    if (waitpid(pid, &wstat, 0) == pid) {
		if (WIFEXITED(wstat)) {
		    status = WEXITSTATUS(wstat);
		}
	    } else {
		putil_syserr(0, "waitpid");
	    }

	    _audit_end("exit", EXITING, status);

	    // Do an explicit _exit( to abort any remaining library
	    // finalizers since they will have run in the child. I.e.
	    // any *previous* finalizers will have run in the
	    // original (now parent) copy and any *subsequent*
	    // ones will have run in the child, so continuing with
	    // exit processing here would run the latter set twice.
	    _exit_real(status);
	}
    } else if (_auditor_isActiveByDefault()) {
	_audit_end("exit", EXITING, 0);
    }

    /*
     * The cleanups below would be done by exit anyway but we try to
     * free things explicitly so that leak detectors such as valgrind
     * can more easily pinpoint real leaks. Perhaps we could have an
     * optimized "production mode" which skips the explicit cleanup.
     */
    code_fini();
    prop_fini();
    vb_fini();
}

/// Interposes over the _exit() function.
/// Most exits are caught by interposed_finalize(), but an explicit
/// call to _exit() skips the destructor, so we catch it here by
/// interposition instead.
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] status   same as the wrapped function
/// @return same as the wrapped function
/*static*/ void
_exit_wrapper(const char *call, void (*next) (int), int status)
{
    WRAPPER_DEBUG("ENTERING _exit_wrapper() => %p [%d]\n", next, status);
    // Apparently exit() and _exit() are separate entry points on Solaris
    // and Linux, but on FreeBSD and OSX exit() calls _exit(). On those
    // platforms we must still intercept _exit() for programs which call
    // it directly but need a guard for other programs so they won't
    // try to send EOA twice.
    if (CurrentCA) {
	_audit_end(call, EXITING, status);
	code_fini();
	prop_fini();
	vb_fini();
    }

    (*next)(status);
}

/// Interposes over the _Exit() function. Where present,
/// _Exit() is defined to have the same semantics as _exit().
/// @param[in] call     the name of the interposed function in string form
/// @param[in] next     a pointer to the interposed function
/// @param[in] status   same as the wrapped function
/// @return same as the wrapped function
/*static*/ void
_Exit_wrapper(const char *call, void (*next) (int), int status)
{
    // Since both signature and semantics are guaranteed identical
    // we can use the _exit() wrapper.
    WRAPPER_DEBUG("ENTERING _Exit_wrapper() => %p [%d]\n", next, status);
    _exit_wrapper(call, next, status);
}
