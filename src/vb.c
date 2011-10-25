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
/// @brief Support for debugging verbosity.
/// This is generally accessed via the -v flag to the client program.

#include "AO.h"

#include "PROP.h"
#include "MOMENT.h"

#include <stdarg.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include <lmcons.h>
#include <process.h>
#include <sys/utime.h>
#else	/*_WIN32*/
#include <grp.h>
#include <pwd.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#endif	/*_WIN32*/

/// @cond static
#define VBMASK_UNSET 		-2

#define VBFLAG_PRIVATE		0x000
#define VBFLAG_PUBLIC		0x001
/// @endcond static

static long VerbosityMask = VBMASK_UNSET;
static FILE *VerbosityStream;

/**************************************************************************
* It's important to keep this array in sync with the related #defines!
**************************************************************************/

// *INDENT-OFF*

static struct {
    CCS		 vb_name;
    int		 vb_flags;
    CCS		 vb_desc;
} vbtab[] = {
    {
	"STD",
	VBFLAG_PUBLIC,
	"Default verbosity ({UP,DOWN}LOADING messages)",
    },
    {
	"TMP",
	VBFLAG_PRIVATE,
	"Undefined: temporary bit for debug work",
    },
    {
	"AG",
	VBFLAG_PRIVATE,
	"Show details of aggregation processing",
    },
    {
	"CA",
	VBFLAG_PRIVATE,
	"Show details of cmdaction processing",
    },
    {
	"CURL",
	VBFLAG_PRIVATE,
	"Show libcurl activities",
    },
    {
	"EXEC",
	VBFLAG_PUBLIC,
	"Show child processes as they are executed",
    },
    {
	"HTTP",
	VBFLAG_PUBLIC,
	"Show headers of all HTTP transactions",
    },
    {
	"MON",
	VBFLAG_PRIVATE,
	"Show each raw audit line as delivered to monitor",
    },
    {
	"PA",
	VBFLAG_PRIVATE,
	"Show details of pathaction processing",
    },
    {
	"SHOP",
	VBFLAG_PUBLIC,
	"Show shopping activities",
    },
    {
	"TIME",
	VBFLAG_PRIVATE,
	"Show HTTP transaction times",
    },
    {
	"URL",
	VBFLAG_PUBLIC,
	"Show just the URLs of HTTP transactions",
    },
    {
	"WHY",
	VBFLAG_PRIVATE,
	"Show the reason a candidate PTX didn't match",
    },
    {
	"RE",
	VBFLAG_PRIVATE,
	"Show what's going on within the RE subsystem",
    },
    {
	"MAP",
	VBFLAG_PRIVATE,
	"Show file map/unmap activity",
    },
    {
	"UP",
	VBFLAG_PRIVATE,
	"Show what's going on within the uploading subsystem",
    },
    {
	"REC",
	VBFLAG_PRIVATE,
	"Report each time the auditor calls the file-access-reporting function",
    },
    {NULL, 0, NULL}
};

// *INDENT-ON*

/// Initializes verbosity.
void
vb_init(void)
{
    // We used to have some logic to allow verbosity to be diverted
    // to a file with the P_VBFILE property but that turned out to
    // get into some catch-22 issues since some verbosity occurs
    // before stdio structures are initialized (theory, unproven).
    // Anyway, it's not the most critical feature so for now at least
    // we always use stderr while leaving the infrastructure in place
    // for a possible reconsideration someday.
    VerbosityStream = stderr;
}

/// Returns the stream to which verbosity should be printed.
/// @return a stdio stream pointer (FILE *)
FILE *
vb_get_stream(void)
{
    return VerbosityStream ? VerbosityStream : stderr;
}

/// Add the named verbosity flags in string form. Verbosity flags are
/// always appended to existing ones, but the special values "-" or
/// "off" reset the state to off. Thus the string "xxx,yyy,-,zzz"
/// would turn on "zzz" verbosity only. If the vbstr parameter is
/// null, all known verbosity terms are printed to stdout.
/// @param[in] vbstr    string representing comma-separated verbosity terms
void
vb_addstr(CCS vbstr)
{
    CCS currval;
    int all;

    if (vbstr) {
	if (*vbstr == '=') {
	    vbstr++;
	}
	all = (*vbstr == '+');
    } else {
	all = 0;
    }

    if (!vbstr || !*vbstr || (*vbstr == '?') || all) {
	int i;

	CCS fmt = "%-8s %s\n";

	fprintf(stdout, fmt, "OFF", "No verbosity messages");
	for (i = 0; vbtab[i].vb_name; i++) {
	    if (all || (vbtab[i].vb_flags & VBFLAG_PUBLIC)) {
		fprintf(stdout, fmt, vbtab[i].vb_name, vbtab[i].vb_desc);
	    }
	}
	exit(0);
    }

    // Append new property strings to the current property value.
    if ((currval = prop_get_str(P_VERBOSITY))) {
	if (*currval) {
	    CS newval = NULL;

	    if (asprintf(&newval, "%s,%s", currval, vbstr) < 0) {
		putil_syserr(2, NULL);
	    }
	    prop_override_str(P_VERBOSITY, newval);
	    putil_free(newval);
	} else {
	    prop_override_str(P_VERBOSITY, vbstr);
	}
    } else {
	prop_put_str(P_VERBOSITY, vbstr);
    }

    // Mark the mask as needing to be recalculated.
    VerbosityMask = VBMASK_UNSET;
}

/// Add the specified verbosity bit to the verbosity mask.
/// @param[in] addbit   an index into the 'vbtab' array
void
vb_addbit(int addbit)
{
    vb_addstr(vbtab[addbit].vb_name);
}

// Internal service routine.
static long
_vb_name2mask(CCS list)
{
    long mask;

    CS t;

    char buf[1024];

    // Default to 'standard' verbosity.
    mask = 1 << VB_STD;

    snprintf(buf, charlen(buf), "%s", list);
    for (t = strtok(buf, ","); t; t = strtok(NULL, ",")) {
	if (!stricmp(t, "OFF") || *t == '-') {
	    mask = 0;
	} else if (!stricmp(t, "ON")) {
	    mask |= (1 << VB_STD);
	} else if (*t == '*') {
	    mask = -1;
	} else {
	    int i;

	    for (i = 0; vbtab[i].vb_name; i++) {
		if (!strnicmp(t, vbtab[i].vb_name, strlen(t))) {
		    mask |= (1 << i);
		    break;
		}
	    }
	    if (!vbtab[i].vb_name) {
		putil_warn("unknown verbosity mask bit name '%s'", t);
	    }
	}
    }

    return mask;
}

/// Determine whether the specified bit is set in the verbosity mask.
/// @param[in] bit      a long specifying a single bit
/// @return nonzero iff the specified bit is set in the verbosity mask
int
vb_bitmatch(long bit)
{
    if (bit == VB_ON) {
	return 1;
    } else if (bit == VB_OFF) {
	return 0;
    } else {
	if (VerbosityMask == VBMASK_UNSET) {
	    VerbosityMask = _vb_name2mask(prop_get_str(P_VERBOSITY));
	}
	return VerbosityMask & (1 << bit) ? 1 : 0;
    }
}

/// Print the specified message to the verbosity stream iff the
/// bit is set.
/// @param[in] bit      a long specifying a single bit
/// @param[in] fmt      a printf-style format string
void
vb_printfA(long bit, const char *fmt, ...)
{
    va_list ap;

    FILE *vbstrm;

    // This might be called from an a DLL startup function before stdio
    // is fully initialized, so we work extra hard to get SOMETHING out
    // even in that case. It may not look good but at least we'd
    // have a clue that the event being reported took place.
    if (vb_bitmatch(bit) && fmt) {
	vbstrm = vb_get_stream();
	if (fprintf(vbstrm, "%s: ", prop_get_app()) < 0) {
	    write(2, prop_get_app(), strlen(prop_get_app()));
	    write(2, ": ", 2);
	}

#if 0
	if (bit != VB_STD && bit != VB_ON) {
	    moment_s now;
	    char nowbuf[MOMENT_BUFMAX];

	    moment_get_systime(&now);
	    (void)moment_format_vb(now, nowbuf, charlen(nowbuf));
	    write(2, nowbuf, strlen(nowbuf));
	    write(2, ": ", 2);

	    // Makes it easier to see extra verbosity or grep it from a log.
	    if (fputc('=', vbstrm) == EOF) {
		write(2, "=", 1);
	    }
	}
#endif

	va_start(ap, fmt);
	if (vfprintf(vbstrm, fmt, ap) < 0) {
	    write(2, fmt, strlen(fmt));
	}
	va_end(ap);

	if (fputc('\n', vbstrm) == EOF) {
	    write(2, "\n", 1);
	}
    }
}

#if defined(_WIN32)
void
vb_printfW(long bit, const wchar_t *fmt, ...)
{
    va_list ap;

    FILE *vbstrm;

    // This may be called from an a DLL startup function before stdio
    // is fully initialized, so we work extra hard to get SOMETHING out
    // even in that case. It may not look good but at least we'd
    // have a clue that the event being reported took place.
    if (vb_bitmatch(bit) && fmt) {
	vbstrm = vb_get_stream();
	if (fprintf(vbstrm, "%s: ", prop_get_app()) < 0) {
	    write(2, prop_get_app(), strlen(prop_get_app()));
	    write(2, ": ", 2);
	}
	// Makes it easier to see extra verbosity or grep it from a log.
	if (bit != VB_STD && bit != VB_ON) {
	    if (fputc('=', vbstrm) == EOF) {
		write(2, "=", 1);
	    }
	}
	va_start(ap, fmt);
	if (vfwprintf(vbstrm, fmt, ap) < 0) {
	    write(2, fmt, wcslen(fmt) * 2);
	}
	va_end(ap);
	if (fputc('\n', vbstrm) == EOF) {
	    write(2, "\n", 1);
	}
    }
}
#endif	/*_WIN32*/

/// Finalizes verbosity.
void
vb_fini(void)
{
    if (VerbosityStream != stderr) {
	(void)fclose(VerbosityStream);
    }
}
