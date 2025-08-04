#include "cJSON.h"

#define DEFAULT "default"
#define EXPORTED "exported"
#define PADDING "padding"
#define VALUE "value"

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
    cJSON *result = _prop_get_key(prop, VALUE);
    if (cJSON_IsNull(result)) {
        result = _prop_get_key(prop, DEFAULT);
    }
    return result;
}

int
_prop_get_pad(CCS prop)
{
    cJSON *item = _prop_get_item(prop);
    // Find out how much padding is needed in the value.
    // This only matters for exported props, usually numeric.
    cJSON *padding = cJSON_GetObjectItem(item, PADDING);
    if (!cJSON_IsNull(padding)) {
        return (int)cJSON_GetNumberValue(padding);
    }
    return 0;
}

bool
prop_is_exported(CCS prop)
{
    cJSON *result = _prop_get_key(prop, EXPORTED);
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
prop_put_str(CCS prop, CCS newval)
{
    cJSON *item = _prop_get_item(prop);
    int pad = prop_is_exported(prop) ? _prop_get_pad(prop) : 0;
    if (pad > 0) {
        CS padval;
        asprintf(&padval, "%*s", pad, newval);
        cJSON_ReplaceItemInObject(item, VALUE, cJSON_CreateString(padval));
        free(padval);
    } else {
        cJSON_ReplaceItemInObject(item, VALUE, cJSON_CreateString(newval));
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
prop_put_long(CCS prop, long newval)
{
    char strbuf[256];  // way longer than any stringified integral value
    (void)snprintf(strbuf, sizeof(strbuf), "%ld", newval);
    (void)prop_put_str(prop, strbuf);
    return prop_get_long(prop);
}

unsigned long
prop_get_ulong(CCS prop)
{
    CCS strval = prop_get_str(prop);
    return strtoul(strval, NULL, 0);
}

unsigned long
prop_put_ulong(CCS prop, unsigned long newval)
{
    char strbuf[256];  // way longer than any stringified integral value
    (void)snprintf(strbuf, sizeof(strbuf), "%lu", newval);
    (void)prop_put_str(prop, strbuf);
    return prop_get_ulong(prop);
}

bool
prop_is_true(CCS prop)
{
    cJSON *value = _prop_get_value(prop);
    assert(cJSON_IsBool(value));
    return cJSON_IsTrue(value);
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
        prop_put_str(prop, value);
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
