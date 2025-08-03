#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cJSON.h"

typedef char *CS;
typedef const char *CCS;

static cJSON *Properties;

#define DEFAULT "default"
#define EXPORTED "exported"
#define PADDING "padding"
#define VALUE "value"

#define putil_free(x) free(x)
#define putil_strdup(x) strdup(x)

void
prop_init(CS path)
{
    if (path) {
        FILE *fp = fopen(path, "r");
        struct stat stbuf;
        fstat(fileno(fp), &stbuf);
        size_t len = (size_t)stbuf.st_size;
        CS json_string = malloc(len + 1);
        fread(json_string, 1, len, fp);
        json_string[len] = '\0';
        fclose(fp);

        cJSON *json = cJSON_Parse(json_string);
        free(json_string);
        Properties = cJSON_GetObjectItem(json, "properties");
    }
}

static cJSON *
_prop_item(CCS prop)
{
    return cJSON_GetObjectItem(Properties, prop);
}

static cJSON *
_prop_key(CCS prop, CCS key)
{
    return cJSON_GetObjectItem(_prop_item(prop), key);
}

static cJSON *
_prop_value(CCS prop)
{
    cJSON *result = _prop_key(prop, VALUE);
    if (cJSON_IsNull(result)) {
        result = _prop_key(prop, DEFAULT);
    }
    return result;
}

CCS
prop_get_str(CCS prop)
{
    cJSON *result = _prop_value(prop);
    if (cJSON_IsNull(result)) {
        return NULL;
    }
    return cJSON_GetStringValue(result);
}

void
_prop_put_str(cJSON *item, CCS newval, int pad)
{
    if (pad > 0) {
        CS padded = NULL;
        asprintf(&padded, "%*s", pad, newval);
        cJSON_ReplaceItemInObject(item, VALUE, cJSON_CreateString(padded));
        putil_free(padded);
    } else {
        cJSON_ReplaceItemInObject(item, VALUE, cJSON_CreateString(newval));
    }
}

int
_prop_get_pad(cJSON *item)
{
    int pad = 0;
    if (cJSON_IsTrue(cJSON_GetObjectItem(item, EXPORTED))) {
        // Find out how much padding is needed in the value.
        // This only matters for exported props, usually numeric.
        cJSON *padding = cJSON_GetObjectItem(item, PADDING);
        if (!cJSON_IsNull(padding)) {
            pad = (int)cJSON_GetNumberValue(padding);
        }
    }
    return pad;
}

CCS
prop_put_str(CCS prop, CCS newval)
{
    cJSON *item = _prop_item(prop);
    _prop_put_str(item, newval, _prop_get_pad(item));
    return prop_get_str(prop);
}

long
prop_get_long(CCS prop)
{
    CCS strval = prop_get_str(prop);
    return strtol(strval, NULL, 0);
}

long
prop_put_long(CCS prop, long newval)
{
    cJSON *item = _prop_item(prop);
    int pad = _prop_get_pad(item);
    CS padded;
    asprintf(&padded, "%*ld", pad, newval);
    (void)prop_put_str(prop, padded);
    return prop_get_long(prop);
}

bool
prop_is_true(CCS prop)
{
    cJSON *value = _prop_value(prop);
    assert(cJSON_IsBool(value));
    return cJSON_IsTrue(value);
}

bool
prop_is_exported(CCS prop)
{
    cJSON *result = _prop_key(prop, EXPORTED);
    return cJSON_IsTrue(result);
}

#if 0
void
prop_export(CCS prop)
{
    cJSON *value = _prop_value(prop);
    if (cJSON_IsNull(value)) {
        return;
    }
    // Turn the property name into a legal ENV_VAR_NAME.
    CCS ev = putil_strdup(prop);
    for (CS *p = ev; *p; p++) {
        if (islower(*p)) {
            *p = toupper(*p);
        } else if (*p == '.') {
            *p = '_';
        }
    }
    // Construct a null-terminated VAR=value string to be exported.
    CCS envstr = NULL;
    if (cJSON_IsString(value)) {
        asprintf(&envstr, "%s=%s", ev, cJSON_GetStringValue(value));
    } else if (cJSON_IsNumber(value)) {
        asprintf(&envstr, "%s=%ld", ev, (long)cJSON_GetNumberValue(value));
    } else if (cJSON_IsBool(value)) {
        asprintf(&envstr, "%s=%s", ev, cJSON_IsTrue(value) ? "true" : "false");
    } else {
        assert(0);
    }
    // TODO - export it and mark it for later freeing.
    putil_free(ev);
}
#endif

void
prop_fini(void)
{
    if (Properties) {
        cJSON_Delete(Properties);
    }
}

int
main(int argc, char **argv)
{
    prop_init(argv[1]);
    for (int i = 2; i < argc; i++) {
        CS prop = argv[i];
        CCS exported = prop_is_exported(prop) ? "True" : "False";
        printf("%s: '%s' [%s]\n", prop, prop_get_str(prop), exported);
        prop_put_str(prop, "Hello");
    }
    for (int i = 2; i < argc; i++) {
        CS prop = argv[i];
        CCS exported = prop_is_exported(prop) ? "True" : "False";
        printf("%s: '%s' [%s]\n", prop, prop_get_str(prop), exported);
    }
    prop_fini();

    return 0;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
