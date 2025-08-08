// Copyright (c) 2005-2025 David Boyce.  All rights reserved.

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
/// @brief Handle client-side makefile generation code.

#include "AO.h"

#include "CA.h"
#include "FREE.h"
#include "HEAP.h"
#include "MAKE.h"
#include "PA.h"
#include "TRY.h"

static CS MakeCmd;
static FILE *MakeFP;

/// Initialize make-dependency and makefile-generation data structures.
void
make_init(CCS exe)
{
    CCS str;
    CCS t;
    CS p;

    // Try getting make to run in .ONESHELL mode. If this build
    // doesn't use make, or uses a version which doesn't know
    // .ONESHELL, no harm is done because the request is made via
    // the environment. But we almost always want make
    // to use .ONESHELL if available because audits can be
    // unreliable without it.
    if (prop_is_true(P_EXECUTE_ONLY)) {
        // This mode should not change the command's behavior at all.
    } else if (prop_is_true(P_MAKE_ONESHELL)) {
        CS exedir, appdir, fragment = NULL;

        if (exe && (exedir = putil_dirname(exe))) {
            if ((appdir = putil_dirname(exedir))) {
                asprintf(&fragment, "%s/etc/ao.mk", appdir);
                FREE(appdir);
            }
            FREE(exedir);
        }

        if (!fragment) {
            putil_warn("missing makefile fragment");
        } else if (access(fragment, R_OK)) {
            putil_syserr(0, fragment);
        } else {
            CS mf;

            if ((mf = getenv("MAKEFILES"))) {
                if (!strstr(mf, fragment)) {
                    CS ev = heap_sprintf("MAKEFILES=%s %s", fragment, mf);
                    putenv(ev);
                }
            } else {
                CS ev = heap_sprintf("MAKEFILES=%s", fragment);
                putenv(ev);
            }
            vb_printf(VB_OFF, "export MAKEFILES=%s", getenv("MAKEFILES"));
        }

        FREE(fragment);
    } else {
        putil_warn(".ONESHELL mode suppressed, disaggregation likely");
    }

    if ((str = prop_get_str(P_MAKE_FILE)) || prop_has_value(P_MAKE_DEPENDS)) {
        asprintf(&MakeCmd, "ao2make");

        if (str) {
            (void)util_substitute_params(str, &t);
            p = MakeCmd;
            asprintf(&MakeCmd, "%s --output='%s'", p, t);
            FREE(p);
            FREE(t);
        }

        p = MakeCmd;
        if ((str = prop_get_str(P_MAKE_DEPENDS))) {
            asprintf(&MakeCmd, "%s --extension=%s", p, str);
        } else {
            asprintf(&MakeCmd, "%s --full", p);
        }
        FREE(p);
    } else {
        return;
    }

    if (prop_is_true(P_MEMBERS_ONLY)) {
        p = MakeCmd;
        asprintf(&MakeCmd, "%s --members-only", p);
        FREE(p);
    }

    if ((str = prop_get_str(P_WFLAG))) {
        CS s2;

        TRY(s2 = strdup(str), "strdup()");
        while ((t = util_strsep(&s2, "\n"))) {
            if (*t == 'm') {
                t += 2;
                if ((p = strchr(t, ','))) {
                    *p = ' ';
                }
                p = MakeCmd;
                asprintf(&MakeCmd, "%s %s", p, t);
                FREE(p);
            }
        }
        FREE(s2);
    }

    p = MakeCmd;
    if ((str = prop_get_str(P_BASE_DIR))) {
        asprintf(&MakeCmd, "%s --base-dir='%s' -", p, str);
    } else {
        asprintf(&MakeCmd, "%s -", p);
    }
    FREE(p);

    if (vb_bitmatch(VB_STD)) {
        fprintf(stderr, "+ %s\n", MakeCmd);
    }

    if (!(MakeFP = popen(MakeCmd, "w"))) {
        exit(2);
    }
}

/// Generate partial Makefile data from a CA object.
/// @param[in] ca       a CmdAction object from which to derive makefile data
void
make_file(ca_o ca)
{
    CCS cmdbuf;

    if (MakeFP && (cmdbuf = ca_to_str(ca))) {
        if (fputs(cmdbuf, MakeFP) == EOF || fputs("\n", MakeFP) == EOF) {
            putil_syserr(0, "fputs(cmdbuf)");
        }
        FREE(cmdbuf);
        fflush(MakeFP);
    } else {
        return;
    }

    return;
}

/// Finalize makefile-generation data structures.
void
make_fini(void)
{
    if (MakeFP && pclose(MakeFP)) {
        putil_syserr(2, MakeCmd);
    }

    FREE(MakeCmd);
}

// vim: sw=4 ts=4 et tw=100 cc=+1
