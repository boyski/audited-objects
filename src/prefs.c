// Copyright (c) 2002-2010 David Boyce.  All rights reserved.

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
/// @brief Support for user-defined preferences.
/// This file basically manages access to the properties table
/// (see prop.c) by implementing a hierarchy of properties locations
/// (look here, then there, then somewhere else, etc). The actual
/// work of reading the properties files is done in prop.c.

#include "AO.h"

#include "PREFS.h"

#include "PROP.h"

/// @cond prefs
#define PROJ_EXT			".project"
/// @endcond prefs

// Searches upward from the current working directory toward the
// filesystem root looking for the named file. Returns the path
// to said file in buf if found, or NULL if not found.
static CS
_find_file_up(CCS name, CS buf, size_t len)
{
    char dir[PATH_MAX];

    if (util_get_cwd(dir, charlen(dir)) == NULL) {
	putil_syserr(2, "util_get_cwd");
    }

    while (strcmp(dir, "/")) {
	char dbuf[PATH_MAX];

	snprintf(buf, len, "%s%s%s", dir, DIRSEP(), name);
	if (!access(buf, F_OK)) {
	    return buf;
	}

	// Quit when we reach the root even if the root is not
	// called '/' (like on Windows).
	putil_dirname(dir, dbuf);
	if (strlen(dbuf) >= strlen(dir)) {
	    break;
	}
	strcpy(dir, dbuf);

#if defined(_WIN32)
	if (!strcmp(dir, "\\")) {
	    break;
	}
#endif	/*_WIN32*/
    }

    buf[0] = '\0';
    return NULL;
}

/// Find all properties files and env vars and set current prefs from them.
/// @param[in] exe      the name of the running executable
/// @param[in] ext      the extension to look for on the properties files
/// @param[in] verbose  verbosity prefix string, or NULL for no verbosity
void
prefs_init(CCS exe, CCS ext, CCS verbose)
{
    char cfgpath[PATH_MAX], cfgpath2[PATH_MAX], cfgname[PATH_MAX];
    char home[PATH_MAX], sysdir[PATH_MAX];
    char progname[PATH_MAX], exedir[PATH_MAX], appdir[PATH_MAX];
    CCS p;

    // Store the short program name for error msgs etc.
    if ((p = strrchr(exe, '/'))) {
	strcpy(progname, p + 1);
#if defined(_WIN32)
    } else if ((p = strrchr(exe, '\\'))) {
	strcpy(progname, p + 1);
#endif	/*_WIN32*/
    } else {
	strcpy(progname, exe);
    }

#if defined(_WIN32)
    {
	CS ext;

	if ((ext = strrchr(progname, '.'))) {
	    *ext = '\0';
	}
    }
#endif	/*_WIN32*/

    prop_put_str(P_PROGNAME, progname);

    // Load initial properties from environment
    prop_load(NULL, verbose);

    if (ext) {
	CCS app;

	app = prop_get_app();

	// Find the directory which establishes the P_BASE_DIR property.
	snprintf(cfgname, charlen(cfgname), ".%s", app);
	if (_find_file_up(cfgname, cfgpath, charlen(cfgpath))) {
	    char pbuf[PATH_MAX];

	    // Look for a properties file in this dir and try loading it.
	    snprintf(cfgpath2, charlen(cfgpath2), "%s%s%s%s",
		cfgpath, DIRSEP(), app, PROP_EXT);

	    prop_load(cfgpath2, verbose);

	    // The location of this file establishes the default
	    // project base.
	    prop_put_str(P_BASE_DIR,
		 putil_canon_path(putil_dirname(cfgpath, pbuf), NULL, 0));
	} else {
	    char cwdbuf[PATH_MAX];

	    // If no project config file exists, use cwd for project base.
	    if (!util_get_cwd(cwdbuf, charlen(cwdbuf))) {
		putil_syserr(2, "util_get_cwd()");
	    }
	    prop_put_str(P_BASE_DIR, putil_canon_path(cwdbuf, NULL, 0));
	}

	// Check for a personal ~/.<app>.properties file. Since
	// Windows makes it hard to create a file with a leading dot
	// (you can do it at the command line but not in the GUI)
	// we also allow the ~/<app>.properties version. And we do that
	// for both U and W to allow for a consistent site standard.
	if (putil_get_homedir(home, charlen(home))) {
	    snprintf(cfgpath, charlen(cfgpath), "%s%s.%s%s",
		       home, DIRSEP(), app, ext);
	    if (!access(cfgpath, F_OK)) {
		prop_load(cfgpath, verbose);
	    } else {
		snprintf(cfgpath, charlen(cfgpath), "%s%s%s%s",
			   home, DIRSEP(), app, ext);
		prop_load(cfgpath, verbose);
	    }
	}

	// Then fall back to system-wide properties from /etc/<app>.properties
	if (putil_get_systemdir(sysdir, charlen(sysdir))) {
	    snprintf(cfgpath, charlen(cfgpath), "%s%s%s%s",
		       sysdir, DIRSEP(), app, ext);
	    prop_load(cfgpath, verbose);
	}

	// Finally, try a truly global properties file from the dir
	// the executable is in (Windows) or ../etc from there (Unix).
	putil_dirname(exe, exedir);
	putil_dirname(exedir, appdir);
#if defined(_WIN32)
	snprintf(cfgpath2, charlen(cfgpath2), "%s%s%s%s",
		   exedir, DIRSEP(), app, ext);
#else	/*_WIN32*/
	snprintf(cfgpath2, charlen(cfgpath2), "%s/etc/%s%s",
		   appdir, app, ext);
#endif	/*_WIN32*/
	if (strcmp(cfgpath2, cfgpath)) {
	    prop_load(cfgpath2, verbose);
	}
    }

    // We need to determine what type of platform this is if not Unix.
#if defined(_WIN32)
    {
	char *term;
	if ((term = putil_getenv("TERM")) && strstr(term, "cygwin")) {
	    prop_put_str(P_CLIENT_PLATFORM, "c");
	} else {
	    prop_put_str(P_CLIENT_PLATFORM, "w");
	}
    }
#endif	/*!_WIN32 */

    // "Strict" is a shorthand for all strict options.
    if (prop_is_true(P_STRICT)) {
	prop_set_true(P_STRICT_UPLOAD);
	prop_set_true(P_STRICT_DOWNLOAD);
	prop_put_long(P_STRICT_ERROR, 1);
	prop_set_true(P_STRICT_AUDIT);
    }

    // This is an opportunity for the user to request that
    // normally-non-fatal errors be promoted to fatal.
    if (prop_has_value(P_STRICT_ERROR)) {
	putil_strict_error(prop_get_long(P_STRICT_ERROR));
    }
}
