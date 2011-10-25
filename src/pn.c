// Copyright (c) 2005-2011 David Boyce.  All rights reserved.

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
/// @brief Definitions for the PathName "class".
/// A PathName represents a pathname, naturally. The main value it
/// adds over a simple string is that it contains both absolute and
/// project-relative versions of the path.
///
/// The term PathName is often abbreviated as PN.

#include "AO.h"

#include "PN.h"
#include "PROP.h"

/// Object structure for describing a pathname.
struct path_name_s {
    CCS pn_abs;			///< canonicalized absolute path
    int pn_rel;			///< project-relative offset into pn_abs
} path_name_s;

static int64_t _pn_path_canon(CCS, CCS, CS, CCS, CCS, int);

static int
_pn_abs_to_project_relative_path(CCS path)
{
    CCS pbase;
    size_t offset;

    pbase =  prop_get_str(P_BASE_DIR);
    offset = strlen(pbase);

    if (offset) {
	if (!pathncmp(path, pbase, offset)) {
	    while (*(path + offset) == '/' || *(path + offset) == '\\') {
		offset++;
	    }
	} else {
	    offset = 0;
	}
    }

    return offset;
}

/// Converts a relative path into an absolute path using project-relative
/// addressing.
/// @param[in] path     a potentially relative pathname
/// @return an absolute pathname
static CCS
_pn_make_project_relative(CCS path)
{
    if (!putil_is_absolute(path)) {
	CS ptmp;

	if (asprintf(&ptmp, "%s/%s", prop_get_str(P_BASE_DIR), path) > 0) {
	    return ptmp;
	} else {
	    putil_syserr(2, NULL);
	}
    }
    return putil_strdup(path);
}

/// Constructor.
/// Accepts either absolute or relative paths as input.
/// Relative paths may be interpreted vs the current working directory
/// or the project base directory depending on the boolean parameter.
/// Note that this does not require the named file to exist; all
/// path work here is entirely abstract.
/// @param[in] path     a string representing the path
/// @param[in] use_cwd  boolean - relative to CWD (true) or RWD (false)
/// @return a new PathName object
pn_o
pn_new(CCS path, int use_cwd)
{
    pn_o pn;

    assert(path);

    if (use_cwd) {
	CCS abspath;
	CS canonpath;

	// Start by making sure we have an absolute path.
	if (putil_is_absolute(path)) {
	    abspath = putil_strdup(path);
	} else {
	    CCS cwd;

	    if ((cwd = util_get_cwd())) {
		if (asprintf((CS *)&abspath, "%s/%s", cwd, path) < 0) {
		    putil_free(cwd);
		    return NULL;
		} else {
		    putil_free(cwd);
		}
	    } else {
		return NULL;
	    }
	}

	// Then canonicalize the path (meaning that all "../"
	// sequences and redundant slashes are removed).
	// Also, backslashes are changed to forward slashes.
	canonpath = (CS)putil_malloc(strlen(abspath) + 1);
	if (_pn_path_canon(abspath, strchr(abspath, '\0') + CHARSIZE,
			canonpath, canonpath + strlen(abspath) + 1,
			"\\/", '/') < 0) {
	    putil_warn("unable to canonicalize '%s'", abspath);
	    putil_free(abspath);
	    putil_free(canonpath);
	    return NULL;
	}
	putil_free(abspath);
	// Finally, set up the object with its canonicalized absolute path
	// plus the PRP offset if any.
	pn = (pn_o)putil_malloc(sizeof(*pn));
	pn->pn_abs = canonpath;
    } else {
	pn = (pn_o)putil_malloc(sizeof(*pn));
	pn->pn_abs = _pn_make_project_relative(path);
    }

    pn->pn_rel = _pn_abs_to_project_relative_path(pn->pn_abs);

    return pn;
}

/// Boolean - returns true iff the path is a member of the project.
/// @param[in] pn       the object pointer
/// @return true or false
int
pn_is_member(pn_o pn)
{
    return pn->pn_rel != 0;
}

/// Boolean - returns true iff the path is currently present on this system.
/// @param[in] pn       the object pointer
/// @return true or false
int
pn_exists(pn_o pn)
{
    return !access(pn->pn_abs, F_OK);
}

/// Returns the absolute version of the pathname.
/// @param[in] pn       the object pointer
/// @return a pointer to the absolute path string
CCS
pn_get_abs(pn_o pn)
{
    return pn->pn_abs;
}

/// Returns a project-relative version of the pathname.
/// @param[in] pn       the object pointer
/// @return a pointer to the relative path string
CCS
pn_get_rel(pn_o pn)
{
    CCS rel;

    rel = pn->pn_abs + pn->pn_rel;
    if (*rel) {
	return rel;
    } else {
	return ".";
    }
}

/// Prints the specified name to standard output, suitably formatted
/// for the platform along with a verb and an optional extended name.
/// @param[in] pn       the object pointer
/// @param[in] verb     a description of what's being done to the path
/// @param[in] ext      an optional extended-naming string
void
pn_verbosity(pn_o pn, CCS verb, CCS ext)
{
    if (vb_bitmatch(VB_STD)) {
	CS tpath;
	CCS plat;

	if (prop_is_true(P_ABSOLUTE_PATHS)) {
	    tpath = putil_strdup(pn_get_abs(pn));
	} else {
	    tpath = putil_strdup(pn_get_rel(pn));
	}

	if ((plat = prop_get_str(P_CLIENT_PLATFORM)) && *plat == 'w') {
	    char *p;

	    for (p = tpath; *p; p++) {
		if (*p == '/') {
		    *p = '\\';
		}
	    }
	}

	if (ext) {
	    vb_printf(VB_STD, "%s %s%s%s", verb, tpath, XNS, ext);
	} else {
	    vb_printf(VB_STD, "%s %s", verb, tpath);
	}

	putil_free(tpath);
    }
}

/// Finalizer - releases all allocated memory for object.
/// @param[in] pn       the object pointer
void
pn_destroy(pn_o pn)
{
    // Free the only string pointer.
    putil_free(pn->pn_abs);

    // Zero-fill the struct before freeing it - nicer for debugging.
    (void)memset(pn, 0, sizeof(*pn));

    // Free the object itself.
    putil_free(pn);
}

/// @cond static
/* FOLLOWING IS taken from libmba 0.9.1 at
 * http://www.ioplex.com/~miallen/libmba/
 *
 * path - manipulate file pathnames
 * Copyright (c) 2003 Michael B. Allen <mba2000 ioplex.com>
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#define ST_START     1
#define ST_SEPARATOR 2
#define ST_NORMAL    3
#define ST_DOT1      4
#define ST_DOT2      5

static int64_t
_pn_path_canon(CCS src, CCS slim, CS dst, CCS dlim, CCS srcsep, int dstsep)
{
    CS start = dst, prev;
    int state = ST_START;

    while (src < slim && dst < dlim) {
	switch (state) {
	    case ST_START:
		state = ST_SEPARATOR;
		if (strchr(srcsep, *src)) {
		    *dst++ = dstsep;
		    src++;
		    break;
		}
	    case ST_SEPARATOR:
		if (*src == '\0') {
		    *dst = '\0';
		    return dst - start;
		} else if (strchr(srcsep, *src)) {
		    src++;
		    break;
		} else if (*src == '.') {
		    state = ST_DOT1;
		} else {
		    state = ST_NORMAL;
		}
		*dst++ = *src++;
		break;
	    case ST_NORMAL:
		if (*src == '\0') {
		    *dst = '\0';
		    return dst - start;
		} else if (strchr(srcsep, *src)) {
		    state = ST_SEPARATOR;
		    *dst++ = dstsep;
		    src++;
		    break;
		}
		*dst++ = *src++;
		break;
	    case ST_DOT1:
		if (*src == '\0') {
		    dst--;
		    *dst = '\0';
		    return dst - start;
		} else if (strchr(srcsep, *src)) {
		    state = ST_SEPARATOR;
		    dst--;
		    break;
		} else if (*src == '.') {
		    state = ST_DOT2;
		    *dst++ = *src++;
		    break;
		}
		state = ST_NORMAL;
		*dst++ = *src++;
		break;
	    case ST_DOT2:
		if (*src == '\0' || strchr(srcsep, *src)) {
		    /* note src is not advanced in this case */
		    state = ST_SEPARATOR;
		    dst -= 2;
		    prev = dst - 1;
		    if (dst == start || prev == start) {
			break;
		    }
		    do {
			dst--;
			prev = dst - 1;
		    } while (dst > start && *prev != dstsep);
		    break;
		}
		state = ST_NORMAL;
		*dst++ = *src++;
		break;
	}
    }

#if !defined(_WIN32)
    errno = ERANGE;
#endif	/*_WIN32*/

    return -1;
}

/// @endcond static
