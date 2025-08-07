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

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "TYPES.h"
#include "JSON.h"
#include "VB.h"

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

// Used to separate a sequence of JSON documents.
// There's another copy in python.
#define SEP "\n=-=-=-=-=-=-="

/// Convert a cJSON object to a printable string
/// @param[in] obj       the cJSON object
/// @return a JSON object containing the same data as the CSV
static CCS
_json_to_str(void *obj)
{
    if (vb_bitmatch(VB_TMP)) {
        return cJSON_Print(obj); // for potentially easier debugging
    }
    return cJSON_PrintUnformatted(obj);
}

/// Free an entire cJSON object recursively
/// @param[in] obj       the cJSON object
static void
_json_delete(void *obj)
{
    cJSON_Delete(obj);
}

/// Parse CSV data into a JSON-formatted string and return it
/// @param[in] csv_data  string containing potentially multiple CSV lines
/// @return a JSON string containing the same data as the CSV
CCS
json_from_ca_csv(CCS ca_csv)
{
    char *copy, *line, *nl, *comma, *fld;
    char *jsonstr;
    int i;

    cJSON *root = cJSON_CreateObject();

    cJSON *calist = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "CommandActions", calist);

    cJSON *palist = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "PathActions", palist);

    for (copy = line = strdup(ca_csv); (nl = strchr(line, '\n')); line = nl + 1) {
        *nl = '\0';
        assert(isdigit(*line) || isupper(*line));
        if isdigit(*line) {
            cJSON *ca = cJSON_CreateObject();
            for (i = 0, fld = line; (comma = strchr(fld, ',')); i++, fld = comma + 1) {
                *comma = '\0';
                cJSON_AddStringToObject(ca, ca_fields[i], fld);
            }
            cJSON_AddStringToObject(ca, ca_fields[i], fld);
            cJSON_AddItemToArray(calist, ca);
        } else {
            cJSON *pa = cJSON_CreateObject();
            for (i = 0, fld = line; (comma = strchr(fld, ',')); i++, fld = comma + 1) {
                *comma = '\0';
                cJSON_AddStringToObject(pa, pa_fields[i], fld);
            }
            cJSON_AddStringToObject(pa, pa_fields[i], fld);
            cJSON_AddItemToArray(palist, pa);
        }
    }
    free(copy);
    jsonstr = (CS)_json_to_str(root);
    _json_delete(root);
    jsonstr = realloc((void *)jsonstr, strlen(jsonstr) + sizeof(SEP));
    (void)strcat(jsonstr, SEP);
    return (CCS)jsonstr;
}

#if 0
/// Compare two cJSON items for content equality, recursively.
/// @param[in] a  "left" json object
/// @param[in] b  "right" json object
/// @return 0 iff a and b differ
int
json_same(const cJSON *a, const cJSON *b)
{
    if (!a || !b) {
        return a == b;
    }

    // Different types are never equal
    if (a->type != b->type) {
        return 0;
    }

    switch (a->type) {
        case cJSON_String:
            return strcmp(a->valuestring, b->valuestring) == 0;

        case cJSON_Number:
            return a->valuedouble == b->valuedouble;

        case cJSON_True:
        case cJSON_False:
        case cJSON_NULL:
            return 1; // Type match is sufficient

        case cJSON_Object: {
            // Compare all key-value pairs
            cJSON *a_item = a->child;
            cJSON *b_item = b->child;

            // Count items first
            if (cJSON_GetArraySize((cJSON*)a) != cJSON_GetArraySize((cJSON*)b)) {
                return 0;
            }

            // Check each key-value pair: is a present in b with same value?
            while (a_item) {
                cJSON *b_match = cJSON_GetObjectItem((cJSON*)b_item, a_item->string);
                if (!b_match || !json_same(a_item, b_match)) {
                    return 0;
                }
                a_item = a_item->next;
            }
            return 1;
        }

        case cJSON_Array: {
            int size_a = cJSON_GetArraySize((cJSON*)a);
            int size_b = cJSON_GetArraySize((cJSON*)b);
            if (size_a != size_b) {
                return 0;
            }

            for (int i = 0; i < size_a; i++) {
                cJSON *item_a = cJSON_GetArrayItem((cJSON*)a, i);
                cJSON *item_b = cJSON_GetArrayItem((cJSON*)b, i);
                if (!json_same(item_a, item_b)) {
                    return 0;
                }
            }
            return 1;
        }

        default:
            return 0;
    }
    return 0;
}
#endif

/// Compare two cJSON items for content equality, recursively.
/// @param[in] a  "left" json object
/// @param[in] b  "right" json object
/// @return 0 iff a and b differ
int
json_same(const void *a, const void *b)
{
    CS str_a, str_b;
    int same;

    str_a = cJSON_PrintUnformatted((cJSON *)a);
    str_b = cJSON_PrintUnformatted((cJSON *)b);
    same = str_a && str_b && (strcmp(str_a, str_b) == 0);
    free(str_a);
    free(str_b);
    return same;
}

/// Parse a null-terminated string containing JSON data.
/// @param[in] json_data  null-terminated string in json format
/// @return a JSON object
json_t *
json_parse(CCS json_data)
{
    return cJSON_Parse(json_data);
}

// vim: sw=4 ts=4 et tw=100 cc=+1
