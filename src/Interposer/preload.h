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

#ifndef _INTERPOSER_PRELOAD_H
#define	_INTERPOSER_PRELOAD_H

#ifdef  __cplusplus
extern "C" {
#endif

#if defined(__APPLE__)
#define PRELOAD_EV	"DYLD_INSERT_LIBRARIES"
#else	/*__APPLE__*/
#define PRELOAD_EV	"LD_PRELOAD"
#endif	/*__APPLE__*/

#define PRELOAD_EV_32	PRELOAD_EV "_32"
#define PRELOAD_EV_64	PRELOAD_EV "_64"

#ifdef  __cplusplus
}
#endif

#endif	/* _INTERPOSER_PRELOAD_H */
