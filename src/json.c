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

#include "Putil/putil.h"

#include "TYPES.h"
#include "FREE.h"
#include "JSON.h"
#include "TRY.h"
#include "VB.h"

#include "cJSON.h"

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
    FREE(copy);
    jsonstr = (CS)_json_to_str(root);
    _json_delete(root);
    jsonstr = realloc((void *)jsonstr, strlen(jsonstr) + sizeof(SEP));
    (void)strcat(jsonstr, SEP);
    return (CCS)jsonstr;
}

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
    same = str_a && str_b && !strcmp(str_a, str_b);
    FREE(str_a);
    FREE(str_b);
    return same;
}

/// Parse a null-terminated string containing JSON data.
/// @param[in] json_data  null-terminated string in json format
/// @return a JSON object
json_t
json_parse(CCS json_data)
{
    return cJSON_Parse(json_data);
}

#ifdef UNIT_TEST_json

typedef struct {
    cJSON *array;
    json_dict_comp_t compare;
} oa_t;

extern int name_compare(const void *a, const void *b);
int
name_compare(const void *a, const void *b)
{
    cJSON *name_a = cJSON_GetObjectItem((cJSON *)a, "name");
    cJSON *name_b = cJSON_GetObjectItem((cJSON *)b, "name");

    assert(cJSON_IsString(name_a) && cJSON_IsString(name_b));
    return strcmp(cJSON_GetStringValue(name_a), cJSON_GetStringValue(name_b));
}

extern int find_insertion_index(oa_t *oa, cJSON *item);
int
find_insertion_index(oa_t *oa, cJSON *item)
{
    int size = cJSON_GetArraySize(oa->array);
    int left = 0, right = size;

    while (left < right) {
        int middle = left + (right - left) / 2;
        cJSON *middle_item = cJSON_GetArrayItem(oa->array, middle);

        int cmp = oa->compare(item, middle_item);
        if (cmp < 0) {
            right = middle;
        } else {
            left = middle + 1;
        }
    }

    return left;
}

extern cJSON* create_person(const char *name, int age);
cJSON*
create_person(const char *name, int age)
{
    cJSON *person = cJSON_CreateObject();
    cJSON_AddStringToObject(person, "name", name);
    cJSON_AddNumberToObject(person, "age", age);
    return person;
}

void *
json_dict_create(json_dict_comp_t compare)
{
    oa_t *oa;

    TRY(oa = malloc(sizeof(oa_t)), "malloc()");
    TRY(oa->array = cJSON_CreateArray(), "cJSON_CreateArray()");
    oa->compare = (json_dict_comp_t)compare;
    return (void *)oa;
}

void
json_dict_add(void *voa, void *vitem)
{
    oa_t *oa = (oa_t *)voa;
    cJSON *item = (cJSON *)vitem;
    int index = find_insertion_index((oa_t *)oa, (cJSON *)item);
    // TRY(cJSON_InsertItemInArray(oa->array, index, item), "cJSON_InsertItemInArray()");
    cJSON_InsertItemInArray(oa->array, index, item);
}

void
json_dict_delete(void *voa)
{
    oa_t *oa = (oa_t *)voa;
    if (oa && oa->array) {
        cJSON_Delete(oa->array);
    }
    FREE(oa);
}

int
main(void)
{
    // Exercise some basic JSON manipulations.
    oa_t *people = (oa_t *)json_dict_create(name_compare);

    json_dict_add(people, create_person("Charlie", 30));
    json_dict_add(people, create_person("Alice", 25));
    json_dict_add(people, create_person("Bob", 35));

    char *json_string = cJSON_Print(people->array);
    printf("%s\n", json_string);
    FREE(json_string);

    json_dict_delete(people);

    return 0;
}
#endif // UNIT_TEST_json

// vim: sw=4 ts=4 et tw=100 cc=+1
