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
json_t *
json_parse(CCS json_data)
{
    return cJSON_Parse(json_data);
}

#ifdef UNIT_TEST_json

static int
name_compare(const void *a, const void *b)
{
    cJSON *name_a = cJSON_GetObjectItem((cJSON *)a, "name");
    cJSON *name_b = cJSON_GetObjectItem((cJSON *)b, "name");

    assert(cJSON_IsString(name_a) && cJSON_IsString(name_b));
    return strcmp(cJSON_GetStringValue(name_a), cJSON_GetStringValue(name_b));
}

static int
_json_dict_index(json_dict_t *json_dict, cJSON *item)
{
    int size = cJSON_GetArraySize(json_dict->array);
    int left = 0, right = size;

    while (left < right) {
        int middle = left + (right - left) / 2;
        cJSON *middle_item = cJSON_GetArrayItem(json_dict->array, middle);

        int cmp = json_dict->compare(item, middle_item);
        if (cmp < 0) {
            right = middle;
        } else {
            left = middle + 1;
        }
    }

    return left;
}

static cJSON*
create_person(const char *name, int age)
{
    cJSON *person = cJSON_CreateObject();
    cJSON_AddStringToObject(person, "name", name);
    cJSON_AddNumberToObject(person, "age", age);
    return person;
}

void *
json_dict_create(CCS name, json_dict_comp_t compare)
{
    json_dict_t *json_dict;

    TRY(json_dict = malloc(sizeof(json_dict_t)), "malloc()");
    TRY(json_dict->array = cJSON_CreateArray(), "cJSON_CreateArray()");
    json_dict->name = name;
    json_dict->compare = (json_dict_comp_t)compare;
    return (void *)json_dict;
}

CCS
json_dict_name(const json_dict_t *vjd)
{
    return vjd->name;
}

void
json_dict_add(const json_dict_t *vjd, const json_t *vitem)
{
    json_dict_t *json_dict = (json_dict_t *)vjd;
    cJSON *item = (cJSON *)vitem;
    int index = _json_dict_index((json_dict_t *)json_dict, (cJSON *)item);
    // TRY(cJSON_InsertItemInArray(json_dict->array, index, item), "cJSON_InsertItemInArray()");
    cJSON_InsertItemInArray(json_dict->array, index, item);
}

void *
json_dict_lookup(const json_dict_t *vjd, const char *name)
{
    json_dict_t *json_dict = (json_dict_t *)vjd;

    cJSON *temp = cJSON_CreateObject();
    cJSON_AddStringToObject(temp, "name", name);
    
    int left = 0, right = cJSON_GetArraySize(json_dict->array) - 1;
    while (left <= right) {
        int middle = left + (right - left) / 2;
        cJSON *middle_item = cJSON_GetArrayItem(json_dict->array, middle);
        
        int cmp = json_dict->compare(temp, middle_item);
        if (cmp == 0) {
            cJSON_Delete(temp);
            return middle_item;
        } else if (cmp < 0) {
            right = middle - 1;
        } else {
            left = middle + 1;
        }
    }
    
    cJSON_Delete(temp);
    return NULL;
}

int
json_dict_count(const json_dict_t *vjd)
{
    json_dict_t *json_dict = (json_dict_t *)vjd;
    return cJSON_GetArraySize(json_dict->array);
}

void
json_dict_delete(void *vjd)
{
    json_dict_t *json_dict = (json_dict_t *)vjd;
    if (json_dict && json_dict->array) {
        cJSON_Delete(json_dict->array);
    }
    FREE(json_dict);
}

int
main(void)
{
    // Exercise some basic JSON manipulations.
    json_dict_t *people = (json_dict_t *)json_dict_create("People", name_compare);

    json_dict_add(people, create_person("Charlie", 30));
    json_dict_add(people, create_person("Alice", 25));
    json_dict_add(people, create_person("Bob", 35));

    char *json_string = cJSON_Print(people->array);
    printf("%s\n", json_string);
    FREE(json_string);

    cJSON *charlie = json_dict_lookup(people, "Charlie");
    char *charlie_string = cJSON_Print(charlie);
    printf("\nCharlie:\n");
    printf("%s\n", charlie_string);
    FREE(charlie_string);

    printf("\nNumber of %s: %d\n", json_dict_name(people), json_dict_count(people));

    json_dict_delete(people);
    return 0;
}
#endif // UNIT_TEST_json

// vim: sw=4 ts=4 et tw=100 cc=+1
