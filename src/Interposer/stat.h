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

#ifndef _INTERPOSER_STAT_H
#define	_INTERPOSER_STAT_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

// Stat calls are a special case due to funky Linux handling.
#ifdef _STAT_VER
#ifdef	sun
WRAP(int, _xstat,   (int ver, const char *path, struct stat *buf), ver, path, buf)
WRAP(int, _lxstat,  (int ver, const char *path, struct stat *buf), ver, path, buf)
WRAP(int, _fxstat,  (int ver, int fildes, struct stat *buf), ver, fildes, buf)
#else	/*sun*/
WRAP(int, __xstat,  (int ver, const char *path, struct stat *buf), ver, path, buf)
WRAP(int, __lxstat, (int ver, const char *path, struct stat *buf), ver, path, buf)
WRAP(int, __fxstat, (int ver, int fildes, struct stat *buf), ver, fildes, buf)
#endif	/*sun*/
#else	/*_STAT_VER*/
WRAP(int,  stat, (const char *path, struct stat *buf), path, buf)
WRAP(int, lstat, (const char *path, struct stat *buf), path, buf)
WRAP(int, fstat, (int fildes, struct stat *buf), fildes, buf)
#endif	/*_STAT_VER*/

// Transitional lf64 stat calls for Solaris and Linux and ?? 32-bit objects.
// Note that Solaris/X86 is very weird: the 32-bit environment looks like
// Linux with *xstat() and _STAT_VER, while the 64-bit environment looks
// like Solaris/Sparc. I.e there are no *xstat64() interfaces.
#if !defined(INTERPOSER_64BIT_MODE)
#if defined(_STAT_VER) && !defined(sun)
WRAP(int, __xstat64,  (int ver, const char *path, struct stat64 *buf), ver, path, buf)
WRAP(int, __lxstat64, (int ver, const char *path, struct stat64 *buf), ver, path, buf)
WRAP(int, __fxstat64, (int ver, int fildes, struct stat64 *buf), ver, fildes, buf)
#else	/*_STAT_VER*/
#if !defined(BSD)
WRAP(int,  stat64, (const char *path, struct stat64 *buf), path, buf)
WRAP(int, lstat64, (const char *path, struct stat64 *buf), path, buf)
WRAP(int, fstat64, (int fildes, struct stat64 *buf), fildes, buf)
#endif	/*BSD*/
#endif	/*_STAT_VER*/
#endif	/*INTERPOSER_64BIT_MODE*/

// This is an additional suite introduced on Solaris.
#ifdef AT_FDCWD
WRAP(int, fstatat, (int fildes, const char *path, struct stat *buf, int flag), fildes, path, buf, flag)
#if !defined(INTERPOSER_64BIT_MODE)
WRAP(int, fstatat64, (int fildes, const char *path, struct stat64 *buf, int flag), fildes, path, buf, flag)
#endif	/*INTERPOSER_64BIT_MODE*/
#endif	/*AT_FDCWD*/

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_STAT_H */
