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
/// @brief Handles client-side makefile generation code.

#include "AO.h"

#include "CA.h"
#include "MAKE.h"
#include "PA.h"

static CS MakeCmd;
static FILE *MakeFP;

/// Initializes make-dependency and makefile-generation data structures.
void
make_init(CCS exe)
{
    CCS str;
    CS p;
    CCS t;

    // Try getting make to run in .ONESHELL mode. If this build
    // doesn't use make, or uses a version which doesn't know
    // .ONESHELL, no harm is done because the request is made via
    // the environment. But we almost always want make
    // to use .ONESHELL if available because audits can be
    // unreliable without it.
    if (prop_is_true(P_MAKE_ONESHELL)) {
	char *exedir, *appdir, *fragment = NULL;

	if (exe && (exedir = putil_dirname(exe))) {
	    if ((appdir = putil_dirname(exedir))) {

#if defined(_WIN32)
		asprintf(&fragment, "%s\\%s.mk", appdir, prop_get_app());
#else	/*_WIN32*/
		asprintf(&fragment, "%s/etc/%s.mk", appdir, prop_get_app());
#endif	/*_WIN32*/
		putil_free(appdir);
	    }
	    putil_free(exedir);
	}

	if (!fragment) {
	    putil_warn("missing makefile fragment");
	} else if (access(fragment, R_OK)) {
	    putil_syserr(0, fragment);
	} else {
	    CS mf, ev;

	    if ((mf = putil_getenv("MAKEFILES"))) {
		asprintf(&ev, "MAKEFILES=%s %s", fragment, mf);
	    } else {
		asprintf(&ev, "MAKEFILES=%s", fragment);
	    }
	    putil_putenv(ev);
	    vb_printf(VB_OFF, "export MAKEFILES=%s\n", putil_getenv("MAKEFILES"));
	}

	putil_free(fragment);
    } else {
	putil_warn(".ONESHELL mode suppressed, disaggregation likely");
    }

    if ((str = prop_get_str(P_MAKE_FILE))
	    || prop_has_value(P_MAKE_DEPENDS)) {
	const char *perl;

	if (!(perl = prop_get_str(P_PERL_CMD)) &&
		!(perl = putil_getenv("PERL"))) {
	    perl = "perl";
	}
	asprintf(&MakeCmd, "%s -S ao2make", perl);

	if (str) {
	    (void)util_substitute_params(str, &t);
	    p = MakeCmd;
	    asprintf(&MakeCmd, "%s --MF=\"%s\"", p, t);
	    putil_free(p);
	    putil_free(t);
	}

	p = MakeCmd;
	if ((str = prop_get_str(P_MAKE_DEPENDS))) {
	    asprintf(&MakeCmd, "%s --ext=%s", p, str);
	} else {
	    asprintf(&MakeCmd, "%s --full", p);
	}
	putil_free(p);
    } else {
	return;
    }

    if (prop_is_true(P_MEMBERS_ONLY)) {
	p = MakeCmd;
	asprintf(&MakeCmd, "%s --members-only", p);
	putil_free(p);
    }

    if ((str = prop_get_str(P_WFLAG))) {
	CS s2;

	s2 = putil_strdup(str);
	while ((t = util_strsep(&s2, "\n"))) {
	    if (*t == 'm') {
		t += 2;
		if ((p = strchr(t, ','))) {
		    *p = ' ';
		}
		p = MakeCmd;
		asprintf(&MakeCmd, "%s %s", p, t);
		putil_free(p);
	    }
	}
	putil_free(s2);
    }

    p = MakeCmd;
    if ((str = prop_get_str(P_BASE_DIR))) {
	asprintf(&MakeCmd, "%s --base=\"%s\" -", p, str);
    } else {
	asprintf(&MakeCmd, "%s -", p);
    }
    putil_free(p);

    if (vb_bitmatch(VB_STD)) {
	fprintf(stderr, "+ %s\n", MakeCmd);
    }

    if (!(MakeFP = popen(MakeCmd, "w"))) {
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
	if (fputs(cmdbuf, MakeFP) == EOF || fputs("\n", MakeFP) == EOF) {
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

    putil_free(MakeCmd);
}
