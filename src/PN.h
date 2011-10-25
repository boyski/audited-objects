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

#ifndef PN_H
#define PN_H

/// @file
/// @brief Declarations for pn.c

/// @cond PN This is an opaque typedef.
typedef struct path_name_s *pn_o;

/// @endcond PN

extern pn_o pn_new(CCS, int);
extern int pn_is_member(pn_o);
extern int pn_exists(pn_o);
extern CCS pn_get_abs(pn_o);
extern CCS pn_get_rel(pn_o);
extern void pn_verbosity(pn_o, CCS, CCS);
extern void pn_destroy(pn_o);

#endif				/*PN_H */
