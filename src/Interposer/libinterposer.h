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

#ifndef _INTERPOSER_LIBINTERPOSER_H
#define	_INTERPOSER_LIBINTERPOSER_H

#ifdef  __cplusplus
extern "C" {
#endif

#include "preload.h"

/****************************************************************************
 * The following two funcs (libinterposer_preload_{on,off}) turn a preloaded
 * library on and off (duh). They help with portability between Unices but
 * are of no use on Windows.
 *   They work by adding to or removing from the LD_PRELOAD EV, but the
 * strategy is complicated by the 32/64 bit models used on various Unices.
 * Basically, on multilib platforms we can't rely directly on LD_PRELOAD
 * due to risk of a "wrong ELF class" error. Instead , on Solaris we use
 * LD_PRELOAD_32 and LD_PRELOAD_64. However, we must *tolerate* an
 * inherited LD_PRELOAD. Thus the technique is that whatever comes in
 * on LD_PRELOAD goes out on *both* LD_PRELOAD_32 and LD_PRELOAD_64.
 * Anything we add ourselves goes only into the 32/64 EVs.
 * However, in order to coexist with other Unix platforms which don't
 * use LD_PRELOAD_{32,64}, we must continue to publish a vanilla
 * LD_PRELOAD which is just a copy of LD_PRELOAD_32; it will be a
 * no-op on Solaris and the only useful LD_* variant elsewhere.
 *   Another Solaris complication: the 32-bit auditing library is always
 * found directly in a lib dir, e.g. ".../lib" while the 64-bit
 * version is in a subdir of that called ".../lib/64". Note that this
 * convention is not parallel to the names of the EV's: on Solaris,
 * while LD_PRELOAD_64 points into ".../lib/64", LD_PRELOAD_32
 * does *not* point into ".../lib/32". Instead it points into the
 * base dir ".../lib".
 *   Net result: LD_PRELOAD always ends up as a copy of LD_PRELOAD_32.
 * The Solaris ld.so.1 will ignore LD_PRELOAD in favor of LD_PRELOAD_32
 * while on other Unix platforms only LD_PRELOAD will end up getting
 * exported. In other words we always *calculate* all three LD_PRELOAD*
 * values; on Solaris we export the _32 and _64 versions and elsewhere
 * we export just the unadorned LD_PRELOAD.
 *   The situation on Linux is quite similar but they have a different
 * solution. The Linux ld.so uses a "dynamic string token" or DST called
 * $LIB. When LD_PRELOAD contains the string $LIB, ld.so will replace
 * it with "lib" or "lib64" depending on the type of the executable.
 * Linux does NOT implement LD_PRELOAD_32 and LD_PRELOAD_64.
 * Note the slight difference in convention: Linux uses "lib" and "lib64"
 * for shared libraries while Solaris uses "lib" and "lib/64".
 *   However, it gets more complex on Linux due to differing strategies
 * between distros. In particular Ubuntu appears to not support $LIB.
 * Whether this is a bug or a design choice is not clear but it's a
 * PITA. Fortunately there's a workaround involving LD_LIBRARY_PATH.
 *   Meanwhile, on Mac OSX the equivalent var is DYLD_INSERT_LIBRARIES
 * and it only works in the presence of DYLD_FORCE_FLAT_NAMESPACE.
 *   On HP-UX 11* the following may be needed for LD_PRELOAD to work:
 *	HP-UX 11.0  : PHSS_26262
 *	HP-UX 11.11 : PHSS_26263
 *	HP-UX ????? : PHSS_22478
 *
 * Got all that?
 ***************************************************************************/

static void
_libinterposer_die(const char *string)
{
    if (string) {
	fprintf(stderr, "INTERPOSER: Error: %s: %s\n", string, strerror(errno));
    } else {
	fprintf(stderr, "INTERPOSER: Error: %s\n", strerror(errno));
    }
    exit(2);
}

/////////////////////////////////////////////////////////////////////////////

// Sometimes useful for debugging purposes but has no "production" value.
static void
libinterposer_preload_dbg(const char *location)
{
    char *pld;

    if ((pld = getenv(PRELOAD_EV_32)))
	fprintf(stderr, "%s: %s='%s'\n", location, PRELOAD_EV_32, pld);
    if ((pld = getenv(PRELOAD_EV_64)))
	fprintf(stderr, "%s: %s='%s'\n", location, PRELOAD_EV_64, pld);
    if ((pld = getenv(PRELOAD_EV)))
	fprintf(stderr, "%s: %s='%s'\n", location, PRELOAD_EV, pld);
}

static char *
_add2path(char *ev, char *prev, char *add)
{
    char *nldp;

    // Currently we put our new stuff after any existing libraries.
    // It remains to be seen whether before or after is a better plan.
    if (prev && *prev) {
	// All documentation says LD_PRELOAD* is a *space* separated
	// list, but that's kind of unusual and in fact colons seem
	// to work fine so we use them. Spaces work fine too though.
	if (trio_asprintf(&nldp, "%s=%s:%s", ev, prev, add) < 0) {
	    _libinterposer_die(NULL);
	}
    } else {
	if (trio_asprintf(&nldp, "%s=%s", ev, add) < 0) {
	    _libinterposer_die(NULL);
	}
    }

    return nldp;
}

static void
libinterposer_preload_on(const char *so, const char *base)
{
    extern char **environ;
    char **ep;
    char *ldp, *ldp32, *ldp64;
    char *nldp, *nldp32, *nldp64;
    char lib64[PATH_MAX], lib32[PATH_MAX];

    // In case the required EVS are present but turned off,
    // restoring their correct spelling should be enough.

    if (!base) {
	for (ep = environ; *ep; ep++) {
	    if (**ep == '_' && (strstr(*ep, PRELOAD_EV + 1) == *ep + 1)) {
		**ep = (PRELOAD_EV)[0];
	    }
	}
	return;
    }

    /*
     * If the required EVS weren't previously present, add them now.
     * Note that Linux has the "dynamic string token" $LIB - see
     * http://www.akkadia.org/drepper/dsohowto.pdf for details.
     * HOWEVER, Ubuntu (at least) seems to disable $LIB so we employ
     * an alternate strategy of setting up LD_LIBRARY_PATH to search
     * for the auditor in both 32- and 64-bit dirs.
     * At the moment, for simplicity, this is done across the board
     * on Linux but it's really required only for Ubuntu, so we could
     * go back to using $LIB on distros where it works if need be.
     */

#if defined(linux) 
    {
	char searchlibs[PATH_MAX * 2];
	char *libpath;

	/*
	 * Handle Linux multilib by using $LD_LIBRARY_PATH to search
	 * both 32- and 64-bit lib dirs. This is because $LIB does not
	 * work on Ubuntu for a reason not yet understood.
	 */

	snprintf(searchlibs, sizeof(searchlibs),
	    "%s/lib64:%s/lib", base, base);
	libpath = _add2path("LD_LIBRARY_PATH",
	    getenv("LD_LIBRARY_PATH"), searchlibs);
	if (putenv(libpath)) {
	    _libinterposer_die("putenv");
	}

	snprintf(lib64, sizeof(lib64), "%s", so);
	snprintf(lib32, sizeof(lib32), "%s", so);
    }
#else
    snprintf(lib64, sizeof(lib64), "%s/lib/64/%s", base, so);
    snprintf(lib32, sizeof(lib32), "%s/lib/%s", base, so);
#endif

    ldp64 = getenv(PRELOAD_EV_64);
    ldp32 = getenv(PRELOAD_EV_32);
    ldp   = getenv(PRELOAD_EV);

    if (ldp && *ldp) {
	// Anything we find on LD_PRELOAD gets promoted to *both*
	// LD_PRELOAD_32 and LD_PRELOAD_64 unless previously eclipsed.
	if (ldp64) {
	    nldp64 = _add2path(PRELOAD_EV_64, ldp64, lib64);
	} else {
	    nldp64 = _add2path(PRELOAD_EV_64, ldp, lib64);
	}
	if (ldp32) {
	    nldp32 = _add2path(PRELOAD_EV_32, ldp32, lib32);
	} else {
	    nldp32 = _add2path(PRELOAD_EV_32, ldp, lib32);
	}
    } else {
	// If we had no LD_PRELOAD coming in, work directly with the
	// 32/64 variants.
	if (ldp64 && *ldp64) {
	    nldp64 = _add2path(PRELOAD_EV_64, ldp64, lib64);
	} else {
	    nldp64 = _add2path(PRELOAD_EV_64, NULL, lib64);
	}
	if (ldp32 && *ldp32) {
	    nldp32 = _add2path(PRELOAD_EV_32, ldp32, lib32);
	} else {
	    nldp32 = _add2path(PRELOAD_EV_32, NULL, lib32);
	}
    }

    // The resulting LD_PRELOAD is just a copy of LD_PRELOAD_32.
    // This is the only preload EV which is actually used except on Solaris.
    if (trio_asprintf(&nldp, "%s=%s", PRELOAD_EV, strchr(nldp32, '=') + 1) < 0) {
	_libinterposer_die(NULL);
    }

#ifdef	sun
    if (putenv(nldp64))
	_libinterposer_die("putenv");
    if (putenv(nldp32))
	_libinterposer_die("putenv");
#else	/*sun*/
    free(nldp64);
    free(nldp32);

    if (putenv(nldp))
	_libinterposer_die("putenv");
#endif	/*sun*/

#if defined(__APPLE__)
    if (putenv("DYLD_FORCE_FLAT_NAMESPACE=1"))
	_libinterposer_die("putenv");
#endif	/*__APPLE__*/
}

/////////////////////////////////////////////////////////////////////////////

static void
libinterposer_preload_off(const char *so, char *const envp[], int all)
{
    extern char **environ;
    char **env;
    size_t solen;

    solen = strlen(so);

    env = envp ? (char **)envp : environ;

    for (; *env; env++) {
	// Match any EV beginning with LD_PRELOAD (on some platforms
	// we have LD_PRELOAD_32 etc.)
	if (!strncmp(*env, PRELOAD_EV, strlen(PRELOAD_EV))) {
	    char *ev, *ldpname, *nbuf, *lib, *ldpath, *e;
	    size_t evlen;

	    if (all) {
		**env = '_';
		continue;
	    }

	    ev = *env;
	    evlen = strlen(ev);

	    // Copy the EV into a buffer we can modify with impunity,
	    // then snip it into a name/value pair.
	    ldpname = malloc(evlen + 2);
	    strcpy(ldpname, ev);
	    ldpath = strchr(ldpname, '=');
	    *ldpath++ = '\0';

	    // Start the new construction buffer with "<EV>=" ...
	    nbuf = malloc(evlen + 2);
	    strcpy(nbuf, ldpname);
	    strcat(nbuf, "=");

	    // Work our way through the space-separated list with strtok().
	    // Actually, since in practice colon-separated lists also work,
	    // we handle them too.
	    // Any path that ends with our shared-object name is skipped,
	    // anything else is copied to the new buffer.
	    // Pathnames containing spaces are SOL, but blame that
	    // on whoever decided LD_PRELOAD* was a space-separated list.
	    for (lib = strtok(ldpath, " :"); lib; lib = strtok(NULL, " :")) {
		e = strchr(lib, '\0');
		if (!streq(e - solen, so)) {
		    strcat(nbuf, lib);
		    strcat(nbuf, " ");
		}
	    }

	    // Trim the trailing space.
	    e = strchr(nbuf, '\0');
	    while (isspace((int)*--e))
		*e = '\0';

	    // If operating on a custom environment, modify it in place.
	    // If operating on the standard 'environ' array, interact
	    // with it via standard OS APIs.
	    // Ideally, esp. when dealing with
	    // an explicit env block, we should shuffle other EVs
	    // down and remove the LD_PRELOAD* slot entirely in
	    // cases where it should be empty. But instead we end
	    // up doing the equivalent of 'putenv("LD_PRELOAD=")'.
	    // This is where we miss the BSD unsetenv() function.
	    if (envp) {
		strcpy(ev, nbuf);
		free(nbuf);
	    } else {
		putenv(nbuf);
	    }

	    free(ldpname);
	}
    }
}

/////////////////////////////////////////////////////////////////////////////

// Because the functions above are made 'static', gcc generates a warning
// if they're not used. If they were made globally visible they'd be
// exported from a shared library, polluting the linker namespace.
// So we make a fake use of them from this fake function, tries to avoid
// the warning by "calling" itself recursively.
void
_libinterposer_never_called(void)
{
    (void)libinterposer_preload_on(NULL, NULL);
    (void)libinterposer_preload_off(NULL, NULL, 0);
    (void)libinterposer_preload_dbg(NULL);
    if ((void *)libinterposer_preload_on > (void *)libinterposer_preload_off) {
	_libinterposer_never_called();
    }
}

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_LIBINTERPOSER_H */
