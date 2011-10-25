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

#ifndef _INTERPOSER_ATTR_H
#define	_INTERPOSER_ATTR_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

WRAP(int, access, (const char *path, int amode), path, amode)
WRAP(int, utime, (const char *path, const struct utimbuf *times), path, times)
WRAP(int, utimes, (const char *path, const struct timeval times[2]), path, times)
WRAP(int, chmod, (const char *path, mode_t mode), path, mode)
WRAP(int, fchmod, (int fildes, mode_t mode), fildes, mode)
WRAP(int, chown, (const char *path, uid_t owner, gid_t group), path, owner, group)
WRAP(int, lchown, (const char *path, uid_t owner, gid_t group), path, owner, group)
WRAP(int, fchown, (int fildes, uid_t owner, gid_t group), fildes, owner, group)

#ifdef AT_FDCWD
WRAP(int, fchownat, (int fildes, const char *path, uid_t owner, gid_t group, int flag), fildes, path, owner, group, flag)
WRAP(int, futimesat, (int fildes, const char *path, const struct timeval times[2]), fildes, path, times)
#endif	/*AT_FDCWD*/

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_ATTR_H */
