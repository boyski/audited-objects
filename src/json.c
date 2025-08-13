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

#include "CCS.h"
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

json_t *
json_init(void)
{
    return cJSON_CreateObject();
}

/// Serialize a cJSON object to a printable string
/// @param[in] obj       the cJSON object
/// @return a string in JSON format
CCS
json_to_str(json_t *obj)
{
    if (vb_bitmatch(VB_TMP)) {
        return cJSON_Print(obj); // for potentially easier debugging
    }
    return cJSON_PrintUnformatted(obj);
}

void
json_fini(json_t *obj)
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
    jsonstr = (CS)json_to_str(root);
    cJSON_Delete(root);
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

static int
_json_dict_index(json_dict_t *json_dict, cJSON *item)
{
    int left = 0, right = cJSON_GetArraySize(json_dict->array);

    while (left < right) {
        int middle = left + (right - left) / 2;
        cJSON *middle_item = cJSON_GetArrayItem(json_dict->array, middle);

        int cmp = json_dict->comparator(item, middle_item);
        if (cmp < 0) {
            right = middle;
        } else {
            left = middle + 1;
        }
    }

    return left;
}

void *
json_dict_create(CCS name, json_dict_comp_t comparator)
{
    json_dict_t *json_dict;

    TRY(json_dict = malloc(sizeof(json_dict_t)), "malloc()");
    TRY(json_dict->array = cJSON_CreateArray(), "cJSON_CreateArray()");
    json_dict->name = strdup(name);
    json_dict->comparator = (json_dict_comp_t)comparator;
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
json_dict_lookup(const json_dict_t *vjd, CCS name)
{
    json_dict_t *json_dict = (json_dict_t *)vjd;

    cJSON *temp = cJSON_CreateObject();
    cJSON_AddStringToObject(temp, "name", name);
    
    int left = 0, right = cJSON_GetArraySize(json_dict->array) - 1;
    while (left <= right) {
        int middle = left + (right - left) / 2;
        cJSON *middle_item = cJSON_GetArrayItem(json_dict->array, middle);
        
        int cmp = json_dict->comparator(temp, middle_item);
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

int
json_dict_iterate(const json_dict_t *vjd, json_callback_t callback, const void *data)
{
    json_dict_t *json_dict = (json_dict_t *)vjd;
    int rc = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, ((cJSON *)json_dict->array)) {
        if (callback) {
            rc += callback(item, data);
        }
    }
    return rc;
}

void
json_dict_delete(const json_t *vjd)
{
    json_dict_t *json_dict = (json_dict_t *)vjd;
    if (json_dict && json_dict->array) {
        cJSON_Delete(json_dict->array);
        FREE(json_dict->name);
    }
    FREE(json_dict);
}

json_hash_t *
json_hash_create(CCS name)
{
    json_hash_t *json_hash;

    TRY(json_hash = malloc(sizeof(json_hash_t)), "malloc()");
    TRY(json_hash->table = cJSON_CreateObject(), "cJSON_CreateObject()");
    json_hash->name = strdup(name);
    return (void *)json_hash;
}

CCS
json_hash_name(const json_hash_t *vjh)
{
    return vjh->name;
}

void
json_hash_insert(const json_t *vjh, CCS name, const void *item)
{
    json_hash_t *json_hash = (json_hash_t *)vjh;
    assert(!cJSON_GetObjectItemCaseSensitive(json_hash->table, name));
    cJSON_AddItemToObject(json_hash->table, name, (cJSON *)item);
}

json_t *
json_hash_lookup(const json_t *vjh, CCS name)
{
    json_hash_t *json_hash = (json_hash_t *)vjh;
    if (json_hash) {
        return cJSON_GetObjectItemCaseSensitive(json_hash->table, name);
    }
    return NULL;
}

int
json_hash_count(const json_hash_t *vjh)
{
    json_hash_t *json_hash = (json_hash_t *)vjh;
    return cJSON_GetArraySize(json_hash->table);
}

int
json_hash_iterate(const json_hash_t *vjh, json_callback_t callback, const void *data)
{
    json_hash_t *json_hash = (json_hash_t *)vjh;
    int rc = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, ((cJSON *)json_hash->table)) {
        if (callback) {
            rc += callback(item, data);
        }
    }
    return rc;
}

void
json_hash_delete(const json_t *vjh)
{
    json_hash_t *json_hash = (json_hash_t *)vjh;
    if (json_hash && json_hash->table) {
        cJSON_Delete(json_hash->table);
        FREE(json_hash->name);
    }
    FREE(json_hash);
}

#ifdef UNIT_TEST_json

static cJSON*
create_creature(const char *name, int age)
{
    cJSON *creature = cJSON_CreateObject();
    cJSON_AddStringToObject(creature, "name", name);
    cJSON_AddNumberToObject(creature, "age", age);
    return creature;
}

static int
per_creature(const json_t *vitem, const void *data)
{
    cJSON *item = (cJSON *)vitem;
    CCS str = cJSON_Print(item);
    if (data) {
        printf("ITEM: %s [%p]\n", str, data);
    } else {
        printf("ITEM: %s\n", str);
    }
    FREE(str);
    return 0;
}

static int
name_compare(const void *a, const void *b)
{
    cJSON *name_a = cJSON_GetObjectItemCaseSensitive((cJSON *)a, "name");
    cJSON *name_b = cJSON_GetObjectItemCaseSensitive((cJSON *)b, "name");

    assert(cJSON_IsString(name_a) && cJSON_IsString(name_b));
    return strcmp(cJSON_GetStringValue(name_a), cJSON_GetStringValue(name_b));
}

int
main(void)
{
    // Exercise some basic json_dict manipulations.
    json_dict_t *people = (json_dict_t *)json_dict_create("People", name_compare);

    json_dict_add(people, create_creature("Loyal", 6));
    json_dict_add(people, create_creature("Leerah", 4));
    json_dict_add(people, create_creature("Lee", 8));
    json_dict_add(people, create_creature("Levi", 11));
    json_dict_add(people, create_creature("Leon", 15));

    char *json_string = cJSON_Print(people->array);
    printf("%s\n", json_string);
    FREE(json_string);

    cJSON *loyal = json_dict_lookup(people, "Loyal");
    char *loyal_string = cJSON_Print(loyal);
    printf("\nLoyal:\n");
    printf("%s\n", loyal_string);
    FREE(loyal_string);

    printf("\nNumber of %s: %d\n", json_dict_name(people), json_dict_count(people));

    printf("\nIterating over People dict:\n");
    json_dict_iterate(people, per_creature, "Hello");

    json_dict_delete(people);

    // Now exercise some basic json_hash manipulations.
    json_hash_t *pets = (json_hash_t *)json_hash_create("Pets");
    json_hash_insert(pets, "Tiny", create_creature("Tiny", 11));
    json_hash_insert(pets, "Scratchy", create_creature("Scratchy", 24));

    printf("\n\nIterating over Pets hash:\n");
    json_hash_iterate(pets, per_creature, "Hello");

    printf("\nNumber of %s: %d\n", json_hash_name(pets), json_hash_count(pets));

    json_hash_delete(pets);

    return 0;
}
#endif // UNIT_TEST_json

// vim: sw=4 ts=4 et tw=100 cc=+1
