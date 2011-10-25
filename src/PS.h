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

#ifndef PS_H
#define PS_H

/// @file
/// @brief Declarations for ps.c

#include "MOMENT.h"
#include "PN.h"

/// @cond PS This is an opaque typedef.
typedef struct path_state_s *ps_o;

/// @endcond PS

extern ps_o ps_new(void);
extern ps_o ps_newFromPath(CCS);
extern ps_o ps_newFromCSVString(CCS);
extern int ps_has_dcode(ps_o);
extern int ps_is_member(ps_o);
extern int ps_is_file(ps_o);
extern int ps_is_dir(ps_o);
extern int ps_is_special(ps_o);
extern int ps_is_link(ps_o);
extern int ps_is_symlink(ps_o);
extern int ps_is_unlink(ps_o);
extern int ps_exists(ps_o);
extern void ps_dcode_cache_init(void);
extern void ps_dcode_cache_fini(void);
extern int ps_stat(ps_o, int);
extern ps_o ps_copy(ps_o);
extern CCS ps_diff(ps_o, ps_o);
extern int ps_toCSVString(ps_o, CS, int);
extern int ps_format_user(ps_o, int, int, CS, int);
extern CCS ps_tostring(ps_o);
extern int ps_set_moment_str(ps_o, CCS);
extern void ps_set_size_str(ps_o, CCS);
extern void ps_set_unlinked(ps_o);
extern void ps_set_symlinked(ps_o);
extern void ps_set_linked(ps_o);
extern void ps_set_dir(ps_o);

GEN_SETTER_GETTER_DECL(ps, moment, moment_s)
GEN_SETTER_GETTER_DECL(ps, size, int64_t)
GEN_SETTER_GETTER_DECL(ps, mode, mode_t)
GEN_SETTER_GETTER_DECL(ps, datatype, int)
GEN_SETTER_GETTER_DECL(ps, dcode, CCS)
GEN_SETTER_GETTER_DECL(ps, fsname, CCS)
GEN_SETTER_GETTER_DECL(ps, pn, pn_o)
GEN_SETTER_GETTER_DECL(ps, pn2, pn_o)
GEN_SETTER_GETTER_DECL(ps, target, CCS)

GEN_GETTER_DECL(ps, abs, CCS)
GEN_GETTER_DECL(ps, rel, CCS)
GEN_GETTER_DECL(ps, abs2, CCS)
GEN_GETTER_DECL(ps, rel2, CCS)

extern void
ps_destroy(ps_o);

#endif				/*PS_H */
