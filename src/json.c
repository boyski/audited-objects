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

#include "Putil/putil.h"

#include "cJSON.h"
#include "JSON.h"

static CCS pa_fields[] = {
    "OP",
    "CALL",
    "TIMESTAMP",
    "PID",
    "DEPTH",
    "PPID",
    "TID",
    "PCCODE",
    "CCODE",
    "DATATYPE",
    "FSNAME",
    "MTIME",
    "SIZE",
    "MODE",
    "DCODE",
    "PATH2",
    "PATH"
};

static CCS ca_fields[] = {
    "PID",
    "DEPTH",
    "PPID",
    "STARTED",
    "DURATION",
    "HOST",
    "RECYCLED",
    "FREETEXT",
    // Divider - CommandAction above, Command below
    "PROG",
    "RWD",
    "PCCODE",
    "CCODE",
    "PATHCODE",
    "CMDLINE"
};

/// Parse a single CSV line and return it as a JSON object.
/// @param[in] columns   table of column names
/// @param[in] line      string containing a CSV line
/// @return a JSON object containing the same data as the CSV
static void *
_json_parse_csv_line(CCS *columns, CCS line)
{
    cJSON *json_obj;
    char *copy, *sep, *field;
    int i;

    json_obj = cJSON_CreateObject();
    for (i = 0, copy = field = putil_strdup(line);
	    (sep = strchr(field, ','));
	    i++, field = sep + 1) {
	*sep = '\0';
	cJSON_AddStringToObject(json_obj, columns[i], field);
    }
    cJSON_AddStringToObject(json_obj, columns[i], field);
    putil_free(copy);

    return json_obj;
}

/// Convert a cJSON object to a printable string
/// @param[in] obj       the cJSON object
/// @return a JSON object containing the same data as the CSV
CCS
json_to_str(void *obj)
{
    char *ev = putil_getenv("OUTPUT_JSON");

    assert(ev);
    if (!atoi(ev)) {
	return cJSON_PrintUnformatted(obj);
    } else {
	return cJSON_Print(obj);
    }
}

/// Free a cJSON object
/// @param[in] obj       the cJSON object
void
json_free(void *obj)
{
    cJSON_Delete(obj);
}

/// Parse CSV data into a JSON-formatted string and return it
/// @param[in] csv_data  string containing potentially multiple CSV lines
/// @return a JSON string containing the same data as the CSV
CCS
json_parse_csv_to_str(CCS csv_data)
{
    cJSON *json_array;
    CCS json_str;
    char *copy, *csv_line, *sep;

    json_array = cJSON_CreateArray();
    for (copy = csv_line = putil_strdup(csv_data);
	    (sep = strchr(csv_line, '\n'));
	    csv_line = sep + 1) {
	cJSON *json_line;
	CCS *columns;

	*sep = '\0';
	columns = isdigit(*csv_line) ? ca_fields : pa_fields;
	if ((json_line = _json_parse_csv_line(columns, csv_line))) {
	    cJSON_AddItemToArray(json_array, json_line);
	}
    }
    putil_free(copy);

    json_str = json_to_str(json_array);

    json_free(json_array);

    return json_str;
}

// vim: sw=4 ts=8 tw=100 cc=+1
