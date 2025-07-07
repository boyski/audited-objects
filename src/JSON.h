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

cJSON *json_parse_csv_line(CCS *, CCS);
cJSON *json_parse_csv_to_array(CCS *, CCS);
CCS json_tostr(cJSON *);
void json_free(cJSON *);

#endif				/*JSON_H */

// vim: sw=4 ts=8 tw=100 cc=+1
