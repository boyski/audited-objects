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
/// @brief Handles client-side makefile generation code.

#include "AO.h"

#include "CA.h"
#include "MAKE.h"
#include "PA.h"

static TCHAR MakeCmd[PATH_MAX * 4 + 256];
static CS MakeFragment;
static FILE *MakeFP;

/// Initializes make-dependency and makefile-generation data structures.
void
make_init(CCS exe)
{
    CCS str;

#if !defined(_WIN32)
    // Try getting make to run in .ONESHELL mode. If this build
    // doesn't use make, or uses a version which doesn't know
    // .ONESHELL, no harm done, but we almost always want make
    // to use .ONESHELL if available.
    if (prop_is_true(P_MAKE_ONESHELL)) {
	TCHAR exedir[PATH_MAX], appdir[PATH_MAX];
	CS ev;

	putil_dirname(exe, exedir);
	putil_dirname(exedir, appdir);

#if defined(_WIN32)
	asprintf(&MakeFragment, "%s\\%s.mk", appdir, prop_get_app());
#else	/*_WIN32*/
	asprintf(&MakeFragment, "%s/etc/%s.mk", appdir, prop_get_app());
#endif	/*_WIN32*/

	if (!_taccess(MakeFragment, F_OK)) {
	    CS mf;

	    if ((mf = putil_getenv("MAKEFILES"))) {
		asprintf(&ev, "MAKEFILES=%s %s", MakeFragment, mf);
	    } else {
		asprintf(&ev, "MAKEFILES=%s", MakeFragment);
	    }
	    putil_putenv(ev);
	} else {
#if !defined(_WIN32)
	    putil_syserr(0, MakeFragment);
#endif	/*_WIN32*/
	    putil_free(MakeFragment);
	    MakeFragment = NULL;
	}
    } else {
	putil_warn(_T("not requesting .ONESHELL mode"));
    }
#endif	/*_WIN32*/

    if ((str = prop_get_str(P_MAKE_FILE))
	    || prop_has_value(P_MAKE_DEPENDS)) {
	TCHAR buf[PATH_MAX];
	const char *perl;

	if (!(perl = prop_get_str(P_PERL_CMD)) &&
		!(perl = putil_getenv("PERL"))) {
	    perl = "perl";
	}
	snprintf(MakeCmd, charlen(MakeCmd), "%s -S ao2make", perl);

	if (str) {
	    util_substitute_params(str, buf, charlen(buf));
	    snprintf(endof(MakeCmd), leftlen(MakeCmd), " --MF=\"%s\"", str);
	}

	if ((str = prop_get_str(P_MAKE_DEPENDS))) {
	    snprintf(endof(MakeCmd), leftlen(MakeCmd), " --ext=%s", str);
	} else {
	    snprintf(endof(MakeCmd), leftlen(MakeCmd), " --full");
	}
    } else {
	return;
    }

    if (prop_is_true(P_MEMBERS_ONLY)) {
	snprintf(endof(MakeCmd), leftlen(MakeCmd), " --members-only");
    }

    if ((str = prop_get_str(P_WFLAG))) {
	CS s2, p, t;

	s2 = (CS)str;
	while ((t = util_strsep(&s2, "\n"))) {
	    if (*t == 'm') {
		snprintf((p = endof(MakeCmd)), leftlen(MakeCmd), " %s", t + 2);
		if ((p = _tcschr(p, ','))) {
		    *p++ = ' ';
		}
	    }
	}
    }

    if ((str = prop_get_str(P_BASE_DIR))) {
	snprintf(endof(MakeCmd), leftlen(MakeCmd), " --base=\"%s\"", str);
    }

    snprintf(endof(MakeCmd), leftlen(MakeCmd), " -");
    if (vb_bitmatch(VB_STD)) {
	fprintf(stderr, "+ %s\n", MakeCmd);
    }

    if (!(MakeFP = _tpopen(MakeCmd, _T("w")))) {
	exit(2);
    }
}

/// Generates partial Makefile data from a command action object.
/// @param[in] ca       a CmdAction object from which to derive makefile data
void
make_file(ca_o ca)
{
    CCS cmdbuf;
    
    if (MakeFP && (cmdbuf = ca_toCSVString(ca))) {
	if (_fputts(cmdbuf, MakeFP) == EOF || _fputts("\n", MakeFP) == EOF) {
	    putil_syserr(0, "fputs(cmdbuf)");
	}
	putil_free(cmdbuf);
	fflush(MakeFP);
    } else {
	return;
    }

    return;
}

/// Finalizes makefile-generation data structures.
void
make_fini(void)
{
    if (MakeFP && pclose(MakeFP)) {
	putil_syserr(2, MakeCmd);
    }

    putil_free(MakeFragment);
}
