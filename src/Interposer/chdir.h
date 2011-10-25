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

#ifndef _INTERPOSER_CHDIR_H
#define	_INTERPOSER_CHDIR_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <Interposer/interposer.h>

// The chdir call is automatically intercepted for reasons explained
// in comments elsewhere. If you want to do something with it you need
// only provide a suitable chdir_wrapper() function.

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_CHDIR_H */
