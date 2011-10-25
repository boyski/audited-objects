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

#ifndef CA_H
#define CA_H

/// @file
/// @brief Declarations for ca.c

#include "MOMENT.h"
#include "PROP.h"
#include "PA.h"

#include "hash.h"

/// @cond CA
typedef struct cmd_key_s *ck_o;
typedef struct cmd_audit_s *ca_o;

/// @endcond CA

extern ck_o ck_new(CCS, unsigned long, unsigned long);
extern ck_o ck_new_from_ca(ca_o);
extern void ck_set_cmdid(ck_o, unsigned long);
extern void ck_destroy(ck_o);
extern ca_o ca_new();
extern ca_o ca_newFromCSVString(CCS);
extern void ca_merge(ca_o, ca_o);
extern void ca_record_pa(ca_o, pa_o);
extern int ca_has_leader(ca_o);
extern int ca_is_top(ca_o);
extern int ca_is_uploadable(ca_o);
extern int ca_get_pa_count(ca_o);
extern int ca_foreach_raw_pa(ca_o, int (*)(pa_o, void *), void *);
extern int ca_foreach_cooked_pa(ca_o, int (*)(pa_o, void *), void *);
extern void ca_write(ca_o, int);
extern void ca_coalesce(ca_o);
extern void ca_start_group(ca_o, int);
extern void ca_aggregate(ca_o, ca_o);
extern void ca_disband(ca_o, void (*)(ca_o));
extern void ca_publish(ca_o, void (*)(ca_o));
extern void ca_derive_pathcode(ca_o);
extern CCS ca_format_header(ca_o);
extern CCS ca_toCSVString(ca_o);
extern int ca_o_hash_cmp(const void *, const void *);
extern hash_val_t ca_o_hash_func(const void *);
extern void ca_clear_pa(ca_o);
extern void ca_destroy(ca_o);

GEN_SETTER_GETTER_DECL(ca, cmdid, unsigned long)
GEN_SETTER_GETTER_DECL(ca, depth, unsigned long)
GEN_SETTER_GETTER_DECL(ca, pcmdid, unsigned long)
GEN_SETTER_GETTER_DECL(ca, starttime, moment_s)
GEN_SETTER_GETTER_DECL(ca, duration, unsigned long)
GEN_SETTER_GETTER_DECL(ca, prog, CCS)
GEN_SETTER_GETTER_DECL(ca, host, CCS)
GEN_SETTER_GETTER_DECL(ca, recycled, CCS)
GEN_SETTER_GETTER_DECL(ca, rwd, CCS)
GEN_SETTER_GETTER_DECL(ca, pccode, CCS)
GEN_GETTER_DECL(ca, ccode, CCS)
GEN_SETTER_GETTER_DECL(ca, pathcode, CCS)
GEN_SETTER_GETTER_DECL(ca, line, CCS)
GEN_SETTER_GETTER_DECL(ca, subs, CCS)
GEN_SETTER_GETTER_DECL(ca, leader, ca_o)
GEN_SETTER_GETTER_DECL(ca, group, void *)
GEN_SETTER_GETTER_DECL(ca, strong, int)
GEN_SETTER_GETTER_DECL(ca, started, int)
GEN_SETTER_GETTER_DECL(ca, closed, int)
GEN_SETTER_GETTER_DECL(ca, processed, int)
GEN_GETTER_DECL(ca, pending, int)

#endif				/*CA_H */
