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

#ifndef MOMENT_H
#define MOMENT_H

/// @file
/// @brief Declarations for moment.c

#include <stdbool.h>
#include <sys/types.h>

/// Structure for storing timestamps (both file and system times) internally.
/// Since time_t is somewhat in flux and its size may vary per
/// platform/compiler/year, and since time_t is a Unix-only concept,
/// we use our own internal format which stores the number of seconds
/// since the Unix epoch in a 64-bit integral type.
typedef struct {
    unsigned long long ntv_sec;      ///< Seconds since the (Unix) epoch
    unsigned long ntv_nsec;          ///< Nanoseconds since ntv_sec.
} moment_s;

extern bool moment_is_set(moment_s);
extern void moment_get_systime(moment_s *);
extern void moment_set_mtime(moment_s *, CCS);
extern int moment_cmp(moment_s, moment_s, moment_s *);
extern int moment_since(moment_s, moment_s *);
extern unsigned long moment_duration(moment_s, moment_s);
extern CCS moment_format(moment_s);
extern int moment_parse(moment_s *, CCS);

#endif // MOMENT_H

// vim: sw=4 ts=4 et tw=100 cc=+1
