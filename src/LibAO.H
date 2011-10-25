// Copyright (c) 2005-2011 David Boyce.  All rights reserved.

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

#ifndef	_LIBAO_H
#define	_LIBAO_H

/// @file
/// @brief Declarations of general use for the Windows auditor

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/// @cond LIBAO

// We don't need the import version of this since all loading is dynamic.
#define LIBAO_API __declspec(dllexport)

#if defined(__cplusplus)
extern "C" {
#endif	/*__cplusplus*/

#define PREMAIN "PreMain"

typedef BOOL(__cdecl * INJECTOR)(LPPROCESS_INFORMATION, LPCWSTR, LPCSTR, LPCSTR);

#if defined(__cplusplus)
}
#endif	/*__cplusplus*/

/// @endcond LIBAO

#endif	/*_LIBAO_H*/
