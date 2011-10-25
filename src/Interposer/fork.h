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

#ifndef _INTERPOSER_FORK_H
#define	_INTERPOSER_FORK_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

WRAP_VOID(pid_t, fork)

#if defined(sun)
WRAP_VOID(pid_t, fork1)
#endif	/*sun*/

#if defined(linux) || defined(__CYGWIN__)
#include <sched.h>
// In newer Linux versions this becomes variadic so we may someday
// need a special-case macro. If we need this wrapper at all, that is.
// WRAP(int, clone, (int (*fn)(void *), void *child_stack, int flags, void *arg),
//      fn, child_stack, flags, arg)
#endif	/*linux*/

// vfork is a very special case. It cannot be wrapped generically
// because of the rule that the function it returns to must not
// return. There may be solutions but they presumably involve hacking
// with the stack in assembler.

// Also, system() and popen() may need to be wrapped because they make
// an intra-library call to fork (or vfork). So if we need to intercept
// forks we will need to handle the implicit forks coming from these.
WRAP(int, system, (const char *str), str)
WRAP(FILE *, popen,  (const char *command, const char *mode), command, mode)

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_FORK_H */
