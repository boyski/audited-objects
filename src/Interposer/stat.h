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

#ifndef _INTERPOSER_STAT_H
#define	_INTERPOSER_STAT_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

// Stat calls are a special case due to funky Linux handling.
#ifdef _STAT_VER
WRAP(int, __xstat,  (int ver, const char *path, struct stat *buf), ver, path, buf)
WRAP(int, __lxstat, (int ver, const char *path, struct stat *buf), ver, path, buf)
WRAP(int, __fxstat, (int ver, int fildes, struct stat *buf), ver, fildes, buf)
#else	/*_STAT_VER*/
WRAP(int,  stat, (const char *path, struct stat *buf), path, buf)
WRAP(int, lstat, (const char *path, struct stat *buf), path, buf)
WRAP(int, fstat, (int fildes, struct stat *buf), fildes, buf)
#endif	/*_STAT_VER*/

#ifdef AT_FDCWD
WRAP(int, fstatat, (int fildes, const char *path, struct stat *buf, int flag), fildes, path, buf, flag)
#endif	/*AT_FDCWD*/

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_STAT_H */

// vim: sw=4 ts=8 tw=100 cc=+1
