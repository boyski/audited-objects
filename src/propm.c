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

int
_prop_get_pad(CCS prop)
{
    cJSON *item = _prop_get_item(prop);
    // Find out how much padding is needed in the value.
    // This only matters for exported props, usually numeric.
    cJSON *padding = cJSON_GetObjectItem(item, "padding");
    if (!cJSON_IsNull(padding)) {
        return (int)cJSON_GetNumberValue(padding);
    }
    return 0;
}

bool
prop_is_exported(CCS prop)
{
    cJSON *result = _prop_get_key(prop, "exported");
    return cJSON_IsTrue(result);
}

CCS
prop_get_str(CCS prop)
{
    cJSON *result = _prop_get_value(prop);
    if (cJSON_IsNull(result)) {
        return NULL;
    }
    return cJSON_GetStringValue(result);
}

CCS
prop_set_str(CCS prop, CCS newval)
{
    cJSON *item = _prop_get_item(prop);
    int pad = prop_is_exported(prop) ? _prop_get_pad(prop) : 0;
    if (pad > 0) {
        CS padval;
        asprintf(&padval, "%*s", pad, newval);
        cJSON_ReplaceItemInObject(item, "value", cJSON_CreateString(padval));
        free(padval);
    } else {
        cJSON_ReplaceItemInObject(item, "value", cJSON_CreateString(newval));
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
prop_is_true(CCS prop)
{
    cJSON *value = _prop_get_value(prop);
    assert(cJSON_IsBool(value));
    return cJSON_IsTrue(value);
}

bool
prop_has_value(CCS prop)
{
    CCS value = _prop_get_value(prop);
    if (value) {
        return *value ? true : false;
    }
    return false;
}

CCS
prop_desc(CCS prop)
{
    return cJSON_GetStringValue(cJSON_GetObjectItem(_prop_get_item(prop), "description"));
}

void
prop_init(CS path)
{
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

    // Environment settings override file-derived properties.
    for (char **envp = environ; *envp; envp++) {
        CS envstr = strdup(*envp);
        if (strncmp(envstr, ENV_PFX, sizeof(ENV_PFX) - 1)) {
            continue;
        }
        CS equals = strchr(envstr, '=');
        *equals = '\0';
        CS value = equals + 1;
        CS prop = envstr + sizeof(ENV_PFX);
        prop_set_str(prop, value);
        free(envstr);
    }

    // Export those properties marked for exporting.
    cJSON *property = NULL;
    cJSON_ArrayForEach(property, Properties) {
        CCS prop = property->string;
        if (prop_is_exported(prop)) {
            CS envstr;
            asprintf(&envstr, "%s%s=%s", ENV_PFX, prop, prop_get_str(prop));
            putenv(envstr);  // TODO will show as memory leak
        }
    }
}

void
prop_fini(void)
{
    cJSON_Delete(Properties);
}

// vim: sw=4 ts=4 et tw=100 cc=+1
