/* Copyright (c) 2005-2025 David Boyce.  All rights reserved. */

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
/// @brief Support for PCRE2 regular expressions.
/// Note that although we try to encapsulate ancillary technologies
/// such as HTTP, regular expressions, compression, hashing, etc.
/// to make it easier to swap out one such library for another,
/// full abstraction is not always possible. That applies here;
/// even though all interaction with PCRE2 is channeled through
/// this file, the regular expressions themselves are going to
/// be in PCRE2 dialect and any change in the RE engine is likely
/// to risk subtle incompatibilities. Fortunately PCRE2 is a very
/// solid technology and there's no reason to think it will ever
/// need to be replaced.

#include "AO.h"

#include "PROP.h"
#include "RE.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

/** @cond static */
/* PCRE2 uses a different approach for output vectors */
#define OVECCNT 15  /* PCRE2 uses pairs - 15 pairs == 30 elements */
/** @endcond static */

/* Documented in the header to avoid triggering some doxygen problem. */
void *
re_init__(CCS name, CCS regex)
{
    pcre2_code *re = NULL;
    PCRE2_SIZE erroffset;
    int errorcode;
    uint32_t opts = PCRE2_UTF;

#if defined(__APPLE__)
    opts |= PCRE2_CASELESS;
#endif // __APPLE__

    vb_printf(VB_RE, "COMPILING %s='%s'", name, regex);

    if (*regex) {
        re = pcre2_compile(
            (PCRE2_SPTR8)regex,      /* pattern */
            PCRE2_ZERO_TERMINATED,   /* length */
            opts,                    /* options */
            &errorcode,              /* error code */
            &erroffset,              /* error offset */
            NULL                     /* compile context */
        );

        if (!re) {
            PCRE2_UCHAR buffer[256];
            pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
            putil_warn("compilation of RE '%s' failed at offset %zu: %s",
                       name, erroffset, buffer);
        }
    }

    return re;
}

/* Documented in the header to avoid triggering some doxygen problem. */
void *
re_init_prop__(CCS prop)
{
    void *result = NULL;

    if (prop_has_value(prop)) {
        CS regex = putil_strdup(prop_get_str(prop));

        /*
         * Allow REs to be specified as e.g. m%regexp%, partly because
         * it looks natural to Perl users and partly to make leading and
         * trailing whitespace possible. However, do NOT treat /regexp/
         * specially because that could confuse users trying to match
         * paths containing (say) the 5 characters "/tmp/".
         */
        if (*regex == 'm') {
            CS start, end;

            if ((start = regex + 1) && (end = strchr(regex, '\0'))) {
                if (--end > start && *start == *end && !ISALPHA(*end)) {
                    *end = '\0';
                    regex += 2;
                }
            }
        }

        result = re_init__(prop, regex);
        putil_free(regex);
    }
    return result;
}

/* Documented in the header to avoid triggering some doxygen problem. */
CCS
re_match__(void *re, CCS str)
{
    pcre2_match_data *match_data;
    PCRE2_SIZE *ovector;
    int rc;
    static char match[256];

    match[0] = '\0';

    /* No RE means no match. */
    if (!re || !str) {
        return NULL;
    }

    /* Create match data block */
    match_data = pcre2_match_data_create_from_pattern((pcre2_code *)re, NULL);
    if (!match_data) {
        return NULL;
    }

    rc = pcre2_match(
        (pcre2_code *)re,           /* compiled pattern */
        (PCRE2_SPTR8)str,           /* subject string */
        strlen(str),                /* subject length */
        0,                          /* start offset */
        0,                          /* options */
        match_data,                 /* match data */
        NULL                        /* match context */
    );

    if (rc < 0) {
        pcre2_match_data_free(match_data);
        return NULL;
    } else {
        ovector = pcre2_get_ovector_pointer(match_data);
        PCRE2_SIZE length = ovector[1] - ovector[0];

        /* Copy the matched substring */
        if (length >= sizeof(match)) {
            length = sizeof(match) - 1;
        }
        memcpy(match, str + ovector[0], length);
        match[length] = '\0';

        pcre2_match_data_free(match_data);
        return match;
    }
}

/* Documented in the header to avoid triggering some doxygen problem. */
void
re_fini__(void **re)
{
    pcre2_code_free((pcre2_code *)*re);
    *re = NULL;
}

CCS 
re_version__(void)
{
    static char version_buffer[64];

    if (pcre2_config(PCRE2_CONFIG_VERSION, version_buffer) >= 0) {
        return version_buffer;
    } else {
        return "PCRE2 (version unknown)";
    }
}

// vim: sw=4 ts=4 et tw=100 cc=+1
