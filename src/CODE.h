// Copyright (c) 2005-2010 David Boyce.  All rights reserved.

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

#ifndef CODE_H
#define CODE_H

/// @file
/// @brief Declarations for code.c

/// The size of a buffer guaranteed big enough for any identity hash.
#define CODE_IDENTITY_HASH_MAX_LEN	2048

extern void code_init(void);
extern CCS code_from_str(CCS, CS, size_t);
extern CCS code_from_buffer(const unsigned char *, off_t, CCS, CS, size_t);
extern CCS code_from_path(CCS, CS, size_t);
extern void code_fini(void);

#endif				/*CODE_H */
