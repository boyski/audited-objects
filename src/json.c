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
/// @brief Support for JSON formatting.
/// Wrapper over the cJSON open-source library.

#include "cJSON.h"
#include "JSON.h"

// Parse a single CSV line and return a JSON object.
cJSON *
json_parse_csv_line(CCS *columns, CCS line)
{
    cJSON *json_obj;
    char *copy, *sep, *field;
    int i;

    json_obj = cJSON_CreateObject();
    for (i = 0, copy = field = strdup(line); (sep = strchr(field, ',')); i++, field = sep + 1) {
	*sep = '\0';
	cJSON_AddStringToObject(json_obj, columns[i], field);
    }
    cJSON_AddStringToObject(json_obj, columns[i], field);
    free(copy);

    return json_obj;
}

// Convert multiple lines of CSV data to a JSON array.
cJSON *
json_parse_csv_to_array(CCS *columns, CCS csv_data)
{
    cJSON *json_array;
    char *copy, *csv_line, *sep, *json;

    json_array = cJSON_CreateArray();
    for (copy = csv_line = strdup(csv_data); (sep = strchr(csv_line, '\n')); csv_line = sep + 1) {
	cJSON *json_line;

	*sep = '\0';
	if ((json_line = json_parse_csv_line(columns, csv_line))) {
	    cJSON_AddItemToArray(json_array, json_line);
	}
    }
    free(copy);

    return json_array;
}

CCS
json_tostr(cJSON *obj)
{
    return cJSON_Print(obj);
}

void
json_free(cJSON *obj)
{
    cJSON_Delete(json_array);
}

// vim: sw=4 ts=8 tw=100 cc=+1
