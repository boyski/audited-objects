// Copyright (c) 2005-2025 David Boyce.  All rights reserved.

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

/// @file
/// @brief Custom typedefs.
///
/// These derive from an earlier iteration which
/// attempted to support wide characters and the tchar.h API on Windows
/// which made abstracted char types useful. But Windows is no longer
/// supported and anyway wide chars are sort of a dead end. UTF-8 looks
/// like the way of the future, but CS and CCS are retained as useful
/// shorthand.

#ifndef CCS_H
#define CCS_H

typedef char *CS;
typedef const char *CCS;

#endif // CCS_H

// vim: sw=4 ts=4 et tw=100 cc=+1
