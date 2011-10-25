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

#ifndef PA_H
#define PA_H

#include "PS.h"

/// @file
/// @brief Declarations for pa.c

/// @cond PA This is an opaque typedef.
typedef struct path_action_s *pa_o;

/// @endcond PA

extern int pa_cmp_by_pathname(const void *, const void *);
extern int pa_cmp(const void *, const void *);
extern pa_o pa_new(void);
extern pa_o pa_newFromCSVString(CCS);
extern int pa_has_dcode(pa_o);
extern int pa_is_member(pa_o);
extern int pa_is_dir(pa_o);
extern int pa_is_special(pa_o);
extern int pa_exists(pa_o);
extern int pa_is_read(pa_o);
extern int pa_is_write(pa_o);
extern int pa_is_link(pa_o);
extern int pa_is_symlink(pa_o);
extern int pa_is_unlink(pa_o);
extern int pa_set_moment_str(pa_o, CCS);
extern int pa_set_timestamp_str(pa_o, CCS);
extern int pa_has_timestamp(pa_o);
extern void pa_set_size_str(pa_o, CCS);
extern CCS pa_toCSVString(pa_o);
extern CCS pa_tostring(pa_o);
extern int pa_stat(pa_o, int);
extern CCS pa_diff(pa_o, pa_o);
extern pa_o pa_copy(pa_o);
extern void pa_destroy(pa_o);

GEN_SETTER_GETTER_DECL(pa, op, op_e)
GEN_SETTER_GETTER_DECL(pa, timestamp, moment_s)
GEN_SETTER_GETTER_DECL(pa, call, CCS)
GEN_SETTER_GETTER_DECL(pa, pid, unsigned long)
GEN_SETTER_GETTER_DECL(pa, ppid, unsigned long)
GEN_SETTER_GETTER_DECL(pa, tid, unsigned long)
GEN_SETTER_GETTER_DECL(pa, depth, unsigned long)
GEN_SETTER_GETTER_DECL(pa, pccode, CCS)
GEN_SETTER_GETTER_DECL(pa, ccode, CCS)
GEN_SETTER_GETTER_DECL(pa, fsname, CCS)
GEN_SETTER_GETTER_DECL(pa, fd, int)
GEN_SETTER_GETTER_DECL(pa, dcode, CCS)
GEN_SETTER_GETTER_DECL(pa, uploadable, int)
GEN_SETTER_GETTER_DECL(pa, ps, ps_o)
GEN_SETTER_GETTER_DECL(pa, moment, moment_s)

GEN_GETTER_DECL(pa, pn, pn_o)
GEN_GETTER_DECL(pa, abs, CCS)
GEN_GETTER_DECL(pa, rel, CCS)
GEN_GETTER_DECL(pa, abs2, CCS)
GEN_GETTER_DECL(pa, rel2, CCS)
GEN_GETTER_DECL(pa, target, CCS)
GEN_GETTER_DECL(pa, datatype, int)
GEN_GETTER_DECL(pa, moment, moment_s)
GEN_GETTER_DECL(pa, size, int64_t)

#endif				/*PA_H */
