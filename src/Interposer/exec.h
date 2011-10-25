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

#ifndef _INTERPOSER_EXEC_H
#define	_INTERPOSER_EXEC_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

// There's no need to write execl_wrapper(), execle_wrapper(), or
// execlp_wrapper(), as execl*() are redirected to the corresponding
// execv*_wrapper() functions.

WRAP(int, execv,  (const char *path, char *const argv[]), path, argv)
WRAP(int, execvp, (const char *file, char *const argv[]), file, argv)
WRAP(int, execve, (const char *path, char *const argv[], char *const envp[]), path, argv, envp)
WRAP_EXECL(int,  , 0)
WRAP_EXECL(int, p, 0)
WRAP_EXECL(int, e, 1)

#if defined(sun)
WRAP(int, isaexec, (const char *path, char *const argv[], char *const envp[]), path, argv, envp)
#endif

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_EXEC_H */
