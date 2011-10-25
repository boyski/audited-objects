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

#ifndef _INTERPOSER_OPEN_H
#define	_INTERPOSER_OPEN_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

// Low level I/O calls.
// The 'open' call is special because of the optional 3rd argument.
WRAP_OPEN(int, open)
WRAP(int, creat, (const char *path, mode_t mode), path, mode)
WRAP(int, dup2, (int fildes1, int fildes2), fildes1, fildes2)
WRAP(int, truncate, (const char *path, off_t length), path, length)
WRAP(int, ftruncate, (int fildes, off_t length), fildes, length)

// Standard I/O calls
WRAP(FILE *, fopen, (const char *path, const char *mode), path, mode)
WRAP(FILE *, freopen, (const char *path, const char *mode, FILE *stream), path, mode, stream)
WRAP(FILE *, fdopen, (int fildes, const char *mode), fildes, mode)

// Transitional lf64 I/O calls for Solaris (and Linux?) 32-bit objects.
// But if we're in a true 64-bit env, no need for *64() interfaces.
// Not all 32-bit binaries have access to *64(); they'd need to have
// been compiled with -D_LARGEFILE_SOURCE (at least on Solaris).
// However, there's no harm in providing hooks for them in all
// 32-bit processes as long as this file itself is compiled with
// -D_LARGEFILE_SOURCE.
#if !defined(INTERPOSER_64BIT_MODE) && defined(_LARGEFILE_SOURCE)
WRAP_OPEN(int, open64)
WRAP(int, creat64, (const char *path, mode_t mode), path, mode)
WRAP(FILE *, fopen64, (const char *path, const char *mode), path, mode)
WRAP(FILE *, freopen64, (const char *path, const char *mode, FILE *stream), path, mode, stream)
#endif

// This is an additional suite introduced on Solaris.
#ifdef AT_FDCWD
WRAP_OPENAT(int, openat)
#if !defined(INTERPOSER_64BIT_MODE)
WRAP_OPENAT(int, openat64)
#endif	/*INTERPOSER_64BIT_MODE*/
#endif	/*AT_FDCWD*/

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_OPEN_H */
