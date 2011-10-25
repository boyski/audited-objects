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

// Transitional lf64 I/O calls for Unix/Linux 32-bit objects.
// The use of these is pretty confused; following is what I believe to
// be the situation. On Solaris, these are available to 32-bit processes
/// only if -D_LARGEFILE_SOURCE was used. In Solaris 64-bit binaries 
// they should never occur because they become compile-time macro
// aliases to the regular names. The situation is the same on Linux
// except that the *64 variants can still be used explicitly (as
// functions, not macros) in a 64-bit binary.
// Bottom line: the rules concerning when these might be needed are
// complex, undocumented, and subject to change/surprise. Since I
// know of no harm that comes from defining the wrappers even when
// they couldn't be used, that's the approach I'd prefer to take.
// But ATM we just hack it for Solaris and Linux and worry about other
// 64-bit platforms as they come alone. This is hacky and should be
// revisited.
#if !defined(sun) || (!defined(INTERPOSER_64BIT_MODE) && defined(_LARGEFILE_SOURCE))
#if !defined(__CYGWIN__)
WRAP_OPEN(int, open64)
WRAP(int, creat64, (const char *path, mode_t mode), path, mode)
WRAP(FILE *, fopen64, (const char *path, const char *mode), path, mode)
WRAP(FILE *, freopen64, (const char *path, const char *mode, FILE *stream), path, mode, stream)
#endif
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
