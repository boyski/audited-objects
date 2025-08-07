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

/// @file
/// @brief Support for Properties objects.
/// TODO

#include "AO.h"
#include "PROP.h"

#include "cJSON.h"

static cJSON *Properties;

static cJSON *
_prop_get_item(CCS prop)
{
    cJSON *item = cJSON_GetObjectItem(Properties, prop);
    return item;
}

static cJSON *
_prop_get_key(CCS prop, CCS key)
{
    return cJSON_GetObjectItem(_prop_get_item(prop), key);
}

static cJSON *
_prop_get_value(CCS prop)
{
    return _prop_get_key(prop, "value");
}

static int
_prop_get_pad(CCS prop)
{
    cJSON *item = _prop_get_item(prop);
    // Find out how much padding is needed in the value.
    // This only matters for exported props, usually numeric.
    cJSON *padding = cJSON_GetObjectItem(item, "padding");
    if (!padding || cJSON_IsNull(padding)) {
        return 0;
    }
    return (int)cJSON_GetNumberValue(padding);
}

bool
prop_is_public(CCS prop)
{
    return prop != NULL;  // TODO - temp hack
}

bool
prop_is_exported(CCS prop)
{
    cJSON *result = _prop_get_key(prop, "exported");
    return cJSON_IsTrue(result);
}

void
prop_export(CCS prop)
{
    CCS value = prop_get_str(prop);
    CS envstr;
    asprintf(&envstr, "%s%s=%s", ENV_PFX, prop, value ? value : "");
    putenv(envstr);  // TODO will show as memory leak
}

CCS
prop_get_app(void)
{
    return basename((CS)prop_get_str(P_MONITOR_PATH));
}

CCS
prop_get_str(CCS prop)
{
    cJSON *result = _prop_get_value(prop);
    if (cJSON_IsNull(result)) {
        return NULL;
    }
    CS strval;
    if (cJSON_IsNumber(result)) {
        asprintf(&strval, "%d", result->valueint);
        return strval;  // TODO needs to be freed
    }
    strval = cJSON_GetStringValue(result);
    return strval ? strval : "";
}

CCS
prop_set_str(CCS prop, CCS newval)
{
    assert(newval);
    bool exported = prop_is_exported(prop);
    cJSON *item = _prop_get_item(prop);
    int pad = exported ? _prop_get_pad(prop) : 0;
    if (pad > 0) {
        CS padval;
        asprintf(&padval, "%*s", pad, newval);
        cJSON_ReplaceItemInObject(item, "value", cJSON_CreateString(padval));
        free(padval);
    } else {
        cJSON_ReplaceItemInObject(item, "value", cJSON_CreateString(newval));
    }
    if (exported) {
        prop_export(prop);
    }
    return prop_get_str(prop);
}

long
prop_get_long(CCS prop)
{
    CCS strval = prop_get_str(prop);
    return strtol(strval, NULL, 0);
}

long
prop_set_long(CCS prop, long newval)
{
    char strbuf[256];  // way longer than any stringified integral value
    (void)snprintf(strbuf, sizeof(strbuf), "%ld", newval);
    (void)prop_set_str(prop, strbuf);
    return prop_get_long(prop);
}

unsigned long
prop_get_ulong(CCS prop)
{
    CCS strval = prop_get_str(prop);
    return strtoul(strval, NULL, 0);
}

unsigned long
prop_set_ulong(CCS prop, unsigned long newval)
{
    char strbuf[256];  // way longer than any stringified integral value
    (void)snprintf(strbuf, sizeof(strbuf), "%lu", newval);
    (void)prop_set_str(prop, strbuf);
    return prop_get_ulong(prop);
}

bool
prop_has_value(CCS prop)
{
    cJSON *item = _prop_get_item(prop);
    
    if (!item || cJSON_IsInvalid(item) || cJSON_IsNull(item)) {
        return false;
    }
    
    cJSON *value = cJSON_GetObjectItem(item, "value");
    if (!value || cJSON_IsInvalid(value) || cJSON_IsNull(value)) {
        return false;
    }

    if (cJSON_IsString(value)) {
        CCS strval = cJSON_GetStringValue(value);
        return (strval && *strval) ? true : false;
    } else if (cJSON_IsNumber(value)) {
        return true;
    } else if (cJSON_IsBool(value)) {
        return true;
    }
    
    return false;
}

bool
prop_is_true(CCS prop)
{
    bool result = false;
    if (prop_has_value(prop)) {
        cJSON *value = _prop_get_key(prop, "value");
        if (cJSON_IsBool(value)) {
            result = cJSON_IsTrue(value);
        } else if (cJSON_IsNumber(value)) {
            result = cJSON_GetStringValue(value) ? true : false;
        } else if (cJSON_IsString(value)) {
            CCS strval = cJSON_GetStringValue(value);
            result = strcmp(strval, "false") && strcmp(strval, "0");
        }
    }
    return result;
}

bool
prop_set_bool(CCS prop, bool on_or_off)
{
    prop_set_long(prop, on_or_off ? 1 : 0);
    return prop_is_true(prop);
}

CCS
prop_desc(CCS prop)
{
    return cJSON_GetStringValue(cJSON_GetObjectItem(_prop_get_item(prop), "description"));
}

void
prop_init(CCS path)
{
    if (path) {
        // Read the specified file into a string in JSON format.
        FILE *fp = fopen(path, "r");
        struct stat stbuf;
        fstat(fileno(fp), &stbuf);
        size_t len = (size_t)stbuf.st_size;
        CS json_string = malloc(len + 1);
        fread(json_string, 1, len, fp);
        json_string[len] = '\0';
        fclose(fp);

        // Parse the JSON string into an actual JSON object.
        cJSON *json = cJSON_Parse(json_string);
        free(json_string);
        Properties = cJSON_GetObjectItem(json, "properties");
    } else {
        Properties = cJSON_CreateObject();
    }

    // Record and export the path to this running program.
    prop_set_str(P_MONITOR_PATH, putil_getexecpath());

    // Environment settings override file-derived properties.
    for (char **envp = environ; *envp; envp++) {
        CS envstr = strdup(*envp);
        if (strncmp(envstr, ENV_PFX, sizeof(ENV_PFX) - 1)) {
            continue;
        }
        CS equals = strchr(envstr, '=');
        *equals = '\0';
        CS value = equals + 1;
        CCS prop = strchr(envstr, '_') + 1;
        if (!strcmp(value, "true")) {
            prop_set_bool(prop, true);
        } else if (!strcmp(value, "false")) {
            prop_set_bool(prop, false);
        } else {
            prop_set_str(prop, value);
        }
        free(envstr);
    }

    // Export those properties marked for exporting.
    cJSON *property = NULL;
    cJSON_ArrayForEach(property, Properties) {
        CCS prop = property->string;
        if (prop_is_exported(prop)) {
            prop_export(prop);
        }
    }
}

/// Finalize properties data.
void
prop_fini(void)
{
    cJSON_Delete(Properties);
}

/// Print the known set of properties and their current values.
/// @param[in] verbose  boolean - if true, show property descriptions
/// @param[in] all      boolean - if true, show internal properties too
void
prop_help(int verbose, int all)
{
    if (verbose) {
        printf("\nPROPERTIES [current values]:\n\n");
    }

    cJSON *property = NULL;
    cJSON_ArrayForEach(property, Properties) {
        CCS prop = property->string;
        if (!(all || prop_is_public(prop))) {
            continue;
        }
        CCS exp = prop_is_exported(prop) ? "*" : "";
        if (verbose) {
            printf("%s%s: %s\n", prop, exp, prop_desc(prop));
        } else {
            puts(prop);
        }
    }
}

void
prop_dump(void)
{
    cJSON *property = NULL;
    cJSON_ArrayForEach(property, Properties) {
        CCS prop = property->string;
        CCS value = prop_get_str(prop);
        if (value) {
            fprintf(stderr, "%s='%s'\n", prop, value);
        } else {
            fprintf(stderr, "%s=\n", prop);
        }
    }
}

#ifdef UNIT_TEST
int
main(void)
{
    prop_init("ao_config.json");
    printf("Starting properties:\n");
    system("env | grep ^AO_");
    printf("\nProperty mods:\n");
    CCS prop = "AUDIT_IGNORE_PATH_RE";
    printf("PROP: %s='%s'\n", prop, prop_get_str(prop));
    printf("PROP: %s='%ld'\n", prop, prop_set_long(prop, UINT_MAX));
    printf("PROP: %s='%lu'\n", prop, prop_set_ulong(prop, INT_MAX));
    printf("PROP: %s='%s'\n", prop, prop_set_str(prop, "Hello"));
    prop = "CSV";
    printf("PROP: %s='%s'\n", prop, prop_get_str(prop));
    printf("PROP: %s='%s'\n", prop, prop_is_true(prop) ? "true" : "false");
    prop_set_bool(prop, true);
    bool answer = prop_is_true(prop);
    printf("PROP: %s='%s'\n", prop, answer ? "true" : "false");
    prop_fini();
    printf("\nEnding properties:\n");
    system("env | grep ^AO_");
    return 0;
}
#endif

// vim: sw=4 ts=4 et tw=100 cc=+1
