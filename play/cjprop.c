#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cJSON.h"

typedef char *CS;
typedef const char *CCS;

static cJSON *Properties;

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
_prop_lookup(CCS prop, CCS key)
{
    cJSON *item = cJSON_GetObjectItem(Properties, prop);
    return cJSON_GetObjectItem(item, key);
}

static cJSON *
_prop_value(CCS prop)
{
    cJSON *result = _prop_lookup(prop, "value");
    if (cJSON_IsNull(result)) {
        result = _prop_lookup(prop, "default");
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

long
prop_get_long(CCS prop)
{
    return (long)cJSON_GetNumberValue(_prop_value(prop));
}

bool
prop_get_bool(CCS prop)
{
    return cJSON_IsTrue(_prop_value(prop));
}

bool
prop_is_exported(CCS prop)
{
    cJSON *result = _prop_lookup(prop, "exported");
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
    // Find out how much padding is needed in the value.
    cJSON *padding = _prop_lookup(prop, "padding");
    int pad = 0;
    if (!cJSON_IsNull(padding)) {
        pad = (unsigned)cJSON_GetNumberValue(padding);
    }
    // Construct a null-terminated VAR=value string to be exported.
    CCS envstr = NULL;
    if (cJSON_IsString(value)) {
        asprintf(&envstr, "%s=%*s", ev, pad, cJSON_GetStringValue(value));
    } else if (cJSON_IsNumber(value)) {
        asprintf(&envstr, "%s=%*ld", ev, pad, (long)cJSON_GetNumberValue(value));
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
    }
    prop_fini();

    return 0;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
