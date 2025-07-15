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

#ifndef _INTERPOSER_LIBINTERPOSER_H
#define	_INTERPOSER_LIBINTERPOSER_H

#ifdef  __cplusplus
extern "C" {
#endif

#include "preload.h"

/****************************************************************************
 * The following two funcs (libinterposer_preload_{on,off}) turn a preloaded
 * library on and off (duh) by adding to or subtracting from the LD_PRELOAD EV.
 * On Mac OSX the equivalent var is DYLD_INSERT_LIBRARIES which only works
 * in the presence of DYLD_FORCE_FLAT_NAMESPACE.
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

    if ((pld = getenv(PRELOAD_EV))) {
	fprintf(stderr, "%s: %s='%s'\n", location, PRELOAD_EV, pld);
    }
}

static void
libinterposer_preload_on(const char *so, const char *base)
{
    char *ldp, *nldp;
    char lib_buf[PATH_MAX];

    (void)snprintf(lib_buf, sizeof(lib_buf), "%s/lib64/%s", base, so);

    ldp = getenv(PRELOAD_EV);

    // The documentation says LD_PRELOAD* is a *space* separated
    // list, but that's kind of unusual and in fact colons seem
    // to work fine so we use them. Spaces work fine too though.
    if (ldp && *ldp) {
	// Currently we put our .so after any existing libraries.
	// It remains to be seen whether before or after is a better plan.
	if (asprintf(&nldp, "%s=%s:%s", PRELOAD_EV, ldp, lib_buf) < 0) {
	    _libinterposer_die(NULL);
	}
    } else {
	if (asprintf(&nldp, "%s=%s", PRELOAD_EV, lib_buf) < 0) {
	    _libinterposer_die(NULL);
	}
    }

    if (putenv(nldp))
	_libinterposer_die("putenv");

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

    env = envp ? (char **)envp : environ;

    for (; *env; env++) {
	// Match any EV beginning with LD_PRELOAD=.
	if (!strncmp(*env, PRELOAD_EV "=", sizeof(PRELOAD_EV) + 1)) {
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
	    ldpname = putil_malloc(evlen + 2);
	    strcpy(ldpname, ev);
	    if (!(ldpath = strchr(ldpname, '='))) {
		return; // can't happen but makes gcc -fanalyzer happier
	    }

	    // Start the new construction buffer with "<EV>=" ...
	    nbuf = putil_malloc(evlen + 2);
	    strcpy(nbuf, ldpname);
	    strcat(nbuf, "=");

	    // Work our way through the space-separated list with strtok().
	    // Since colon-separated lists also work they're handled too.
	    // Paths ending with our shared-object name are skipped,
	    // anything else is copied to the new buffer.
	    // Pathnames containing spaces are SOL but blame that
	    // on whoever decided LD_PRELOAD* was a space-separated list.
	    for (lib = strtok(ldpath, " :"); lib; lib = strtok(NULL, " :")) {
		if (!endswith(lib, so)) {
		    strcat(nbuf, lib);
		    strcat(nbuf, " ");
		}
	    }

	    // Trim trailing space.
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
extern void _libinterposer_never_called(void);
void
_libinterposer_never_called(void)
{
    (void)libinterposer_preload_on(NULL, NULL);
    (void)libinterposer_preload_off(NULL, NULL, 0);
    (void)libinterposer_preload_dbg(NULL);
    if (0) {
	_libinterposer_never_called();
    }
}

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_LIBINTERPOSER_H */

// vim: sw=4 ts=8 tw=100 cc=+1
