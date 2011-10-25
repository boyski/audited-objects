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

#ifndef MOMENT_H
#define MOMENT_H

/// @file
/// @brief Declarations for moment.c

#include <sys/types.h>

/// Structure for storing timestamps (both file and system times) internally.
/// Since time_t is somewhat in flux and its size may vary per
/// platform/compiler/year, and since time_t is a Unix-only concept,
/// we use our own internal format which stores the number of seconds
/// since the Unix epoch in a 64-bit integral type. Though it doesn't
/// in theory need to be signed, since time_t is signed we use a signed
/// type too which avoids some signed/unsigned comparison warning,
typedef struct {
    int64_t ntv_sec;		///< Seconds since the (Unix) epoch
    long ntv_nsec;		///< Nanoseconds since ntv_sec.
} moment_s;

/// A buffer size guaranteed sufficient to hold a formatted moment.
#define MOMENT_BUFMAX		32

extern int moment_is_set(moment_s);
extern int moment_get_systime(moment_s *);
extern int moment_set_mtime(moment_s *, CCS);
extern int moment_cmp(moment_s, moment_s, moment_s *);
extern unsigned long moment_duration(moment_s, moment_s);
extern int moment_parse(moment_s *, CCS);
extern CCS moment_format(moment_s, CS, size_t);
extern CCS moment_format_vb(moment_s, CS, size_t);
extern CCS moment_format_id(moment_s *, CS, size_t);

#endif				/*MOMENT_H */
