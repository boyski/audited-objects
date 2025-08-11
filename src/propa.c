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
/// @brief Support for Properties objects.
/// TODO

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdbool.h>  // TODO - needed until -std=c2x is reached

#include "TYPES.h"
#include "FREE.h"
#include "PROP.h"

#include "properties.h"

static CS
_prop_getenv(CCS prop)
{
    CS env_var;
    asprintf(&env_var, "%s%s", ENV_PFX, prop);
    CS value = getenv(env_var);
    FREE(env_var);
    return value;
}

CCS
prop_get_app(void)
{
    return basename((CS)prop_get_str(P_MONITOR_PATH));
}

CCS
prop_get_str(CCS prop)
{
    return _prop_getenv(prop);
}

CCS
prop_set_str(CCS prop, CCS newval)
{
    CS oldval = _prop_getenv(prop);
    size_t oldlen = strlen(oldval);
    size_t newlen = strlen(newval);
#if 0
    if (newlen > oldlen) {
        fprintf(stderr, "%s: OLD='%s' NEW='%s'\n", prop, oldval, newval);
    }
#endif
    assert(newlen <= oldlen);
    (void)strncpy(oldval, newval, oldlen);
    oldval[newlen] = '\0';
    return prop_get_str(prop);
}

long
prop_get_long(CCS prop)
{
    CCS strval = prop_get_str(prop);
    return strtol(strval, NULL, 0);
}

long
prop_set_long(CCS prop, long newval)
{
    CS oldval = _prop_getenv(prop);
    assert(oldval);
    char newbuf[256];  // much longer than any stringified integral value
    (void)snprintf(newbuf, sizeof(newbuf), "%*ld", (int)strlen(oldval), newval);
    prop_set_str(prop, newbuf);
    return prop_get_long(prop);
}

unsigned long
prop_get_ulong(CCS prop)
{
    CCS strval = prop_get_str(prop);
    return strtoul(strval, NULL, 0);
}

unsigned long
prop_set_ulong(CCS prop, unsigned long newval)
{
    CS oldval = _prop_getenv(prop);
    assert(oldval);
    char newbuf[256];  // much longer than any stringified integral value
    (void)snprintf(newbuf, sizeof(newbuf), "%*lu", (int)strlen(oldval), newval);
    prop_set_str(prop, newbuf);
    return prop_get_ulong(prop);
}

bool
prop_has_value(CCS prop)
{
    return _prop_getenv(prop) ? true : false;
}

bool
prop_is_true(CCS prop)
{
    CS oldval = _prop_getenv(prop);
    assert(oldval);
    return strcmp(oldval, "true") == 0;
}

bool
prop_set_bool(CCS prop, bool on_or_off)
{
    CS oldval = _prop_getenv(prop);
    assert(oldval && *oldval);
    (void)strcpy(oldval, on_or_off ? "true" : "false");
    return prop_is_true(prop);
}

void
prop_dump(void)
{
    system("set -x; env | grep ^AO_ 1>&2");
}

#ifdef UNIT_TEST_propa
int
main(void)
{
    // Show auditor properties from the environment.
    char needbuf[256];
    (void)snprintf(needbuf, sizeof(needbuf), "%lu", ULONG_MAX);
    for (char **envp = environ; *envp; envp++) {
        if (!strncmp(*envp, ENV_PFX, sizeof(ENV_PFX) - 1)) {
            char *prop = strdup(*envp + 3);
            char *oldval = strchr(prop, '=');
            *oldval++ = '\0';
            printf(" ENV: %s='%s'\n", prop, oldval);
            printf("PROP: %s='%s'\n", prop, prop_get_str(prop));
            if (strlen(oldval) >= strlen(needbuf)) {
                printf("PROP: %s=%ld\n", prop, prop_set_long(prop, UINT_MAX));
                printf("PROP: %s=%lu\n", prop, prop_set_ulong(prop, INT_MAX));
            } else {
                printf("PROP: %s too small for %s\n", prop, needbuf);
            }
            if (strlen(oldval) >= strlen("Hello")) {
                printf("PROP: %s='%s'\n", prop, prop_set_str(prop, "Hello"));
            } else {
                printf("PROP: %s no room for string\n", prop);
            }
            FREE(prop);
        }
    }

    return 0;
}
#endif // UNIT_TEST_propa

// vim: sw=4 ts=4 et tw=100 cc=+1
