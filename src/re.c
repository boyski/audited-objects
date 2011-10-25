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
/// @brief Support for PCRE regular expressions.
/// Note that although we try to encapsulate ancillary technologies
/// such as HTTP, regular expressions, compression, hashing, etc.
/// to make it easier to swap out one such library for another,
/// full abstraction is not always possible. That applies here;
/// even though all interaction with PCRE is channeled through
/// this file, the regular expressions themselves are going to
/// be in PCRE dialect and any change in the RE engine is likely
/// to risk subtle incompatibilities. Fortunately PCRE is a very
/// solid technology and there's no reason to think it will ever
/// need to be replaced.

#include "AO.h"

#include "PROP.h"
#include "RE.h"

#include "pcre.h"

/// @cond static
// See pcreapi man page and pcredemo.c sample program (www.pcre.org).
#define OVECCNT					30
/// @endcond static

// Documented in the header to avoid triggering some doxygen bug.
void *
re_init__(prop_e prop)
{
    char *restr;
    pcre *re = NULL;

    if ((restr = (char *)prop_get_str(prop)) && *restr && !ISSPACE(*restr)) {
	const char *error;
	int erroffset;
	int opts = PCRE_UTF8;

#if defined(_WIN32)
	opts |= PCRE_CASELESS;
#endif	/*_WIN32*/

	// Allow REs to be specified as e.g. m%regexp%, partly because
	// it looks natural to Perl users and partly to make leading and
	// trailing whitespace possible. However, do NOT treat /regexp/
	// specially because that could confuse users trying to match
	// paths containing (say) the 5 characters "/tmp/".
	if (*restr == 'm') {
	    char *start, *end;

	    if ((start = restr + 1) && (end = strchr(restr, '\0'))) {
		if (--end > start && *start == *end && !ISALPHA(*end)) {
		    *end = '\0';
		    restr += 2;
		}
	    }
	}

	vb_printf(VB_RE, _T("COMPILING %s='%s'"), prop_to_name(prop), restr);

	if (!(re = pcre_compile(restr, opts, &error, &erroffset, NULL))) {
	    putil_warn(_T("compilation of RE '%s' failed at offset %d: %s"),
		       prop_get_str(prop), erroffset, error);
	}
    }

    return re;
}

// Documented in the header to avoid triggering some doxygen bug.
CCS
re_match__(void *re, CCS str)
{
    int ovec[OVECCNT];
    int rc;
    static TCHAR match[256];

    match[0] = '\0';

    // No RE means no match.
    if (!re || !str) {
	return NULL;
    }

    rc = pcre_exec((pcre *) re, NULL, str, (int)_tcslen(str),
		   0, 0, ovec, OVECCNT);
    if (rc < 0) {
	return NULL;
    } else {
	(void)pcre_copy_substring(str, ovec, rc, 0, match, charlen(match));
	return match;
    }
}

// Documented in the header to avoid triggering some doxygen bug.
void
re_fini__(void **re)
{
    pcre_free(*re);
    *re = NULL;
}
