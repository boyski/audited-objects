// Copyright (c) 2002-2025 David Boyce.  All rights reserved.

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

#ifndef PROP_H
#define PROP_H

/// @file
/// @brief Declarations shared by propm.c and propa.c

extern char **environ;

// Hard-wired because monitor and auditor must agree.
#define ENV_PFX "AO_"

bool prop_is_public(CCS);
bool prop_is_exported(CCS);
void prop_export(CCS);
CCS prop_get_app(void);
CCS prop_get_str(CCS);
CCS prop_set_str(CCS, CCS);
long prop_get_long(CCS);
long prop_set_long(CCS, long);
unsigned long prop_get_ulong(CCS);
unsigned long prop_set_ulong(CCS, unsigned long);
bool prop_has_value(CCS);
bool prop_is_true(CCS);
bool prop_set_bool(CCS, bool);
CCS prop_desc(CCS);
void prop_init(CCS, CCS);
void prop_fini(void);
void prop_help(int, int);
void prop_dump(void);

#endif // PROP_H

// vim: sw=4 ts=4 et tw=100 cc=+1
