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

#ifndef _INTERPOSER_EXIT_H
#define	_INTERPOSER_EXIT_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

// As I understand it, "normal" exit processing unloads each
// shared library which in turns causes its finalizer (aka
// destructor) function to be called. Normal exit processing
// includes calls to exit() as well as a simple return from
// the main function. Abnormal exiting would be calls to the
// _exit() or _Exit() functions as well as kill-by-signal
// (such as core dumps). _Exit is defined by C99 while _exit
// is traditional Unix.

// We don't care to trace aborts but we do have to handle both
// normal and abnormal exits. Normal exits are handled not by
// interposition but by providing a finalizer, because returns
// main do not make an explicit call to any exit function
// and thus can't be caught via interposition. However, for
// abnormal exits the finalizer will not be called. Fortunately,
// those involve an explicit call to _exit() and can be handled
// by interposition.

// Our interceptor used to have the noreturn attribute (below) to
// avoid a gcc warning.  But it turns out this breaks the wrapping of
// _exit() somehow and the warning went away anyway. Strange. Maybe a
// bug in an old gcc version we no longer use? Anyway, shown here
// commented out for historical purposes.
//static void _exit_wrapper(const char *call, void (*next)(int), int)
//      __attribute__ ((noreturn));

WRAP_NORETURN(void, _exit, (int status), status)

WRAP_NORETURN(void, _Exit, (int status), status)

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_EXIT_H */
