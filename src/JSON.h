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

#ifndef JSON_H
#define JSON_H

/// @file
/// @brief Declarations for json.c

///////////////////////////////////////////////////////////////////////////////
// The goal is to avoid exposing the cJSON API outside of json.c so void
// types are used instead.

typedef void json_t;

// Standard comparator type to enforce ordering.
typedef int (*json_dict_comp_t)(const void *, const void *);

typedef struct {
    CCS name;
    json_t *array;
    json_dict_comp_t comparator;
} json_dict_t;

///////////////////////////////////////////////////////////////////////////////

CCS json_from_ca_csv(CCS);
int json_same(const void *, const void *);
json_t *json_parse(CCS);

// Intended as a replacement for the Kazlib dict_*() API.
json_t *json_dict_create(CCS, json_dict_comp_t);
CCS json_dict_name(const json_dict_t *);
void json_dict_add(const json_dict_t *, const json_t *);
json_t *json_dict_lookup(const json_dict_t *, const char *);
int json_dict_count(const json_dict_t *);
void json_dict_delete(json_t *);

#endif // JSON_H

// vim: sw=4 ts=4 et tw=100 cc=+1
