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

/// @file
/// @brief Support for user-defined preferences.
/// This file manages access to the properties table (see prop.c)
/// by implementing a hierarchy of places to search for property
/// files. The work of reading properties files is done in prop.c.

#include "AO.h"
#include "HEAP.h"
#include "PREFS.h"
#include "PROP.h"
#include "RE.h"

#include <wordexp.h>

/// @cond prefs
#define PROJ_EXT                        ".project"
/// @endcond prefs

// Search upward from the current working directory toward the
// filesystem root looking for the named file. Return the path
// to said file in buf if found or NULL if not found.
static CS
_find_file_up(CCS name)
{
    CS dir, result;

    if (!(dir = (CS)util_get_cwd())) {
        putil_syserr(0, "util_get_cwd");
        exit(2);
    }

    while (strcmp(dir, "/")) {
        CS pdir;

        TRY(asprintf(&result, "%s%s%s", dir, DIRSEP(), name), "asprintf()");
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
    }

    putil_free(dir);
    return NULL;
}

/// Find all properties files and env vars and set current prefs from them.
/// @param[in] ext      the extension to look for on the properties files
/// @param[in] verbose  verbosity prefix string, or NULL for no verbosity
void
prefs_init(CCS ext, CCS verbose)
{
    CS cfgpath;
    char *exedir;
    CCS p;

    // Load initial properties from environment.
    // Since prop values are sticky these will usually win.
    prop_load(NULL, verbose, 0);

    if (ext) {
        CCS app;
        CS cfgname;
        CS home;
        CS sysdir;
        size_t len;

        app = prop_get_app();
        len = putil_path_max() + 1;

        // Check for a personal ~/.<app>.properties file.
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

        // Try a truly global properties file at ../etc from the directory
        // the executable is in.
        if ((exedir = putil_dirname(ProgramName))) {
            CS globalcfg = NULL;
            CS appdir;

            if ((appdir = putil_dirname(exedir))) {
                asprintf(&globalcfg, "%s/etc/%s%s", appdir, app, ext);
                putil_free(appdir);
            }

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

            // The location of this file establishes the default project base.
            if ((pdir = putil_dirname(cfgpath))) {
                prop_put_str(P_BASE_DIR, pdir);
                putil_free(pdir);
            }
            putil_free(cfgpath);
        } else {
            CCS cwd;

            // If no project config file exists, use cwd for project base.
            if ((cwd = util_get_cwd())) {
                prop_put_str(P_BASE_DIR, cwd);
                putil_free(cwd);
            } else {
                putil_syserr(2, "util_get_cwd()");
            }
        }
        putil_free(cfgname);
    }

    // "Strict" is a convenience shorthand for all strict options.
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

/// Finalize prefs data.
void
prefs_fini(void)
{
    // Nothing to do here.
}

// vim: sw=4 ts=4 et tw=100 cc=+1
