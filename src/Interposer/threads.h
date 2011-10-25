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

#ifndef _INTERPOSER_THREADS_H
#define	_INTERPOSER_THREADS_H

#ifdef  __cplusplus
extern "C" {
#endif

// Support for POSIX threads.

#include <pthread.h>

#include <Interposer/interposer.h>

WRAP(int, pthread_create,
    (pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_func)(void*), void *arg),
    thread, attr, start_func, arg)

WRAP_NORETURN(void, pthread_exit, (void *value_ptr), value_ptr)

// Support for old-fashioned Sun threads.

#if defined(sun)

#include <thread.h>

WRAP(int, thr_create,
    (void *stack_base, size_t stack_size,
	void *(*start_func)(void*), void *arg, long flags, thread_t *new_ID),
    stack_base, stack_size, start_func, arg, flags, new_ID)

WRAP_NORETURN(void, thr_exit, (void *status), status)

#endif	/*sun*/

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_THREADS_H */
