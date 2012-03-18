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

/// @file
/// @brief Support for user-defined preferences.
/// This file basically manages access to the properties table
/// (see prop.c) by implementing a hierarchy of properties locations
/// (look here, then there, then somewhere else, etc). The actual
/// work of reading the properties files is done in prop.c.

#include "AO.h"

#include "PREFS.h"

#include "PROP.h"
#include "RE.h"

#if !defined(_WIN32)
#include <wordexp.h>
#endif	/*_WIN32*/

/// @cond prefs
#define PROJ_EXT			".project"
/// @endcond prefs

// Searches upward from the current working directory toward the
// filesystem root looking for the named file. Returns the path
// to said file in buf if found, or NULL if not found.
static CS
_find_file_up(CCS name)
{
    CS dir;
    CCS project_base_glob;
    CS result;

    if (!(dir = (CS)util_get_cwd())) {
	putil_syserr(2, "util_get_cwd");
    }

    project_base_glob = prop_get_str(P_PROJECT_BASE_GLOB);

    while (strcmp(dir, "/")) {
	char *glob;
	char *pdir;

	// Optionally, break when the specified glob is matched.
	if (project_base_glob) {
#if defined(_WIN32)
	    WIN32_FIND_DATA FindFileData;
	    HANDLE hFind;

	    asprintf(&glob, "%s/%s", dir, project_base_glob);
	    if ((hFind = FindFirstFile(glob, &FindFileData)) != INVALID_HANDLE_VALUE) {
		asprintf(&result, "%s\\%s", dir, putil_basename(FindFileData.cFileName));
		FindClose(hFind);
		putil_free(glob);
		putil_free(dir);
		return result;
	    }
	    putil_free(glob);
#else	/*_WIN32*/
	    wordexp_t wexp;
	    CS scratch;
	    CS g;

	    scratch = putil_strdup(project_base_glob);

	    for (g = util_strsep(&scratch, " "); g; g = util_strsep(&scratch, " ")) {
		asprintf(&glob, "%s/%s", dir, g);
		switch (wordexp(glob, &wexp, WRDE_NOCMD)) {
		    case 0:
			if (wexp.we_wordc > 0 && !access(wexp.we_wordv[0], F_OK)) {
			    asprintf(&result, "%s/%s", dir, putil_basename(wexp.we_wordv[0]));
			    wordfree(&wexp);
			    putil_free(glob);
			    putil_free(dir);
			    return result;
			} else {
			    wordfree(&wexp);
			}
			break;
		    case WRDE_NOSPACE:
			wordfree(&wexp);
			break;
		    default:
			break;
		}
		putil_free(glob);
	    }

	    putil_free(scratch);
#endif	/*_WIN32*/
	}

	asprintf(&result, "%s%s%s", dir, DIRSEP(), name);
	if (!access(result, F_OK)) {
	    putil_free(dir);
	    return result;
	} else {
	    putil_free(result);
	}

	// Quit when we reach the root even if the root is not
	// called '/' (like on Windows).
	if ((pdir = putil_dirname(dir))) {
	    if (strlen(pdir) >= strlen(dir)) {
		putil_free(pdir);
		break;
	    } else {
		putil_free(dir);
		dir = pdir;
	    }
	}

#if defined(_WIN32)
	if (!strcmp(dir, "\\")) {
	    break;
	}
#endif	/*_WIN32*/
    }

    putil_free(dir);
    return NULL;
}

/// Find all properties files and env vars and set current prefs from them.
/// @param[in] exe      the name of the running executable
/// @param[in] ext      the extension to look for on the properties files
/// @param[in] verbose  verbosity prefix string, or NULL for no verbosity
void
prefs_init(CCS exe, CCS ext, CCS verbose)
{
    CS cfgpath;
    CS progname;
    char *exedir;
    CCS p;

    // Store the short program name for error msgs etc.
    if ((p = strrchr(exe, '/'))) {
	progname = putil_strdup(p + 1);
#if defined(_WIN32)
    } else if ((p = strrchr(exe, '\\'))) {
	progname = putil_strdup(p + 1);
#endif	/*_WIN32*/
    } else {
	progname = putil_strdup(exe);
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
    putil_free(progname);

    // Load initial properties from environment
    prop_load(NULL, verbose, 0);

    if (ext) {
	CCS app;
	CS cfgname;
	CS home;
	CS sysdir;
	size_t len;

	app = prop_get_app();
	len = putil_path_max() + 1;

	// Check for a personal ~/.<app>.properties file. Since
	// Windows makes it hard to create a file with a leading dot
	// (you can do it at the command line but not in the GUI)
	// we also allow the ~/<app>.properties version. And we do that
	// for both U and W to allow for a consistent site standard.
	home = (CS)putil_malloc(len);
	if (putil_get_homedir(home, len)) {
	    asprintf(&cfgpath, "%s%s.%s%s", home, DIRSEP(), app, ext);
	    if (!access(cfgpath, F_OK)) {
		prop_load(cfgpath, verbose, 0);
	    } else {
		putil_free(cfgpath);
		asprintf(&cfgpath, "%s%s%s%s", home, DIRSEP(), app, ext);
		prop_load(cfgpath, verbose, 0);
	    }
	    putil_free(cfgpath);
	}
	putil_free(home);

	// Then fall back to system-wide properties from /etc/<app>.properties
	sysdir = (CS)putil_malloc(len);
	if (putil_get_systemdir(sysdir, len)) {
	    asprintf(&cfgpath, "%s%s%s%s", sysdir, DIRSEP(), app, ext);
	    prop_load(cfgpath, verbose, 0);
	    putil_free(cfgpath);
	}
	putil_free(sysdir);

	// Try a truly global properties file from the dir
	// the executable is in (Windows) or ../etc from there (Unix).
	if ((exedir = putil_dirname(exe))) {
#if defined(_WIN32)
	    CS globalcfg;

	    asprintf(&globalcfg, "%s%s%s%s", exedir, DIRSEP(), app, ext);
#else	/*!_WIN32*/
	    CS globalcfg;
	    CS appdir;

	    if ((appdir = putil_dirname(exedir))) {
		asprintf(&globalcfg, "%s/etc/%s%s", appdir, app, ext);
		putil_free(appdir);
	    }
#endif	/*_WIN32*/

	    putil_free(exedir);

	    prop_load(globalcfg, verbose, 0);

	    putil_free(globalcfg);
	}

	// Finally, find a directory to establish the P_BASE_DIR property.
	// This one wins over all previous settings.
	asprintf(&cfgname, ".%s", app);
	if ((cfgpath = _find_file_up(cfgname))) {
	    char *pdir;
	    CS bdcfg;

	    // Look for a properties file in the base dir and try loading it.
	    asprintf(&bdcfg, "%s%s%s%s", cfgpath, DIRSEP(), app, PROP_EXT);
	    prop_load(bdcfg, verbose, 1);
	    putil_free(bdcfg);

	    // The location of this file establishes the default
	    // project base.
	    if ((pdir = putil_dirname(cfgpath))) {
		prop_put_str(P_BASE_DIR,
		    putil_canon_path(pdir, NULL, 0));
		putil_free(pdir);
	    }
	    putil_free(cfgpath);
	} else {
	    CCS cwd;

	    // If no project config file exists, use cwd for project base.
	    if ((cwd = util_get_cwd())) {
		prop_put_str(P_BASE_DIR, putil_canon_path((CS)cwd, NULL, 0));
		putil_free(cwd);
	    } else {
		putil_syserr(2, "util_get_cwd()");
	    }
	}
	putil_free(cfgname);
    }

    // We need to determine what type of platform this is if not Unix.
#if defined(_WIN32)
    {
	char *term;
	if ((term = putil_getenv("TERM")) && strstr(term, "cygwin")) {
	    prop_put_str(P_MONITOR_PLATFORM, "c");
	} else {
	    prop_put_str(P_MONITOR_PLATFORM, "w");
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

    return;
}
