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

#ifndef _INTERPOSER_LINK_H
#define	_INTERPOSER_LINK_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

WRAP(int, link, (const char *path1, const char *path2), path1, path2)
WRAP(int, symlink, (const char *name1, const char *name2), name1, name2)
WRAP(int, rename, (const char *old, const char *newpath), old, newpath)
WRAP(int, unlink, (const char *path), path)
WRAP(int, mkdir,  (const char *path, mode_t mode), path, mode)
WRAP(int, mkdirp, (const char *path, mode_t mode), path, mode)
WRAP(int, rmdir,  (const char *path), path)

#ifdef AT_FDCWD
WRAP(int, renameat, (int fromfd, const char *oldpath, int tofd, const char *newpath), fromfd, oldpath, tofd, newpath)
WRAP(int, linkat,   (int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags), olddirfd, oldpath, newdirfd, newpath, flags)
WRAP(int, unlinkat, (int dirfd, const char *path, int flag), dirfd, path, flag)
#endif	/*AT_FDCWD*/

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_LINK_H */
