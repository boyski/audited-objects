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

#ifndef _INTERPOSER_ALLOC_H
#define	_INTERPOSER_ALLOC_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

WRAP(void *, malloc,  (size_t size), size)
WRAP(void *, calloc,  (size_t nelem, size_t elsize), nelem, elsize)
WRAP(void *, realloc, (void *ptr, size_t size), ptr, size)
WRAP(void *, valloc,  (size_t size), size)
WRAP(void,   free,    (void *ptr), ptr)

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_ALLOC_H */
