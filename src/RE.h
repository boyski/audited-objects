// Copyright (c) 2002-2011 David Boyce.  All rights reserved.

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

#ifndef RE_H
#define RE_H

/// @file
/// @brief Declarations for re.c

// These functions MUST NOT be called by the obvious names 're_init'
// and 're_match' because there are some programs (in GNU coreutils
// at least) which use those names. Ideally we would limit exports
// from the auditor to prevent such problems across the board but
// that's a painful manual task which tends to bite you every time
// you add a new interposition, and may not be an option on every
// platform.

/// Reads a regular expression from the specified property and returns
/// the compiled form ready for passing to re_match__(). If the property
/// has no value, or its value is a zero-length string, or begins
/// with whitespace, NULL is returned. The whitespace check is
/// required by the way we have to treat environment variables
/// within the auditor.
/// NOTE: the extraneous underscores are to prevent conflict with
/// existing APIs.
/// @param[in] prop     a property representing a regular expression
/// @return a compiled regular expression or NULL
extern void *re_init__(prop_e prop);

/// Matches the provided regular expression against the provided string.
/// Returns the matched substring or NULL on no match. If either
/// parameter is NULL, the result is NULL. 
/// NOTE: the <I>value</I> of the returned match is not thread-safe
/// because it is written to a static buffer. When used in a boolean
/// (match/no match) fashion, however, the result can be considered
/// thread-safe. The matched substring is only present for debugging
/// purposes anyway.
/// NOTE: the extraneous underscores are to prevent conflict with
/// existing APIs.
/// @param[in] re       the pointer returned from a re_init__() call
/// @param[in] str      a string to compare with
/// @return the matched substring, or NULL if no match
extern CCS re_match__(void *re, CCS str);

/// Frees the compiled regular expression when no longer needed.
/// The pointer is freed and set to NULL.
/// NOTE: the extraneous underscores are to prevent conflict with
/// existing APIs.
/// @param[in,out] re      a pointer to the regular expression
extern void re_fini__(void **re);

#endif				/*RE_H */
