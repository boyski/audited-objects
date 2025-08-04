#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef char *CS;
typedef const char *CCS;

extern char **environ;

// Hard-wired because the client and auditor must agree.
#define ENV_PFX "AO_"

CS
_prop_getenv(CCS prop)
{
    CS env_var;
    asprintf(&env_var, "%s%s", ENV_PFX, prop);
    CCS value = getenv(env_var);
    free(env_var);
    return value;
}

#include "propa.c"

#include "cJSON.h"
static cJSON *Properties;

#define DEFAULT "default"
#define EXPORTED "exported"
#define PADDING "padding"
#define VALUE "value"

#define putil_strdup(x) strdup(x)

#include "propm.c"

int
main(int argc, char **argv)
{
    (void)argc;
    prop_init(argv[1]);

    // Show monitor properties from file.
    cJSON *property = NULL;
    cJSON_ArrayForEach(property, Properties) {
        CCS prop = property->string;
        printf("%s: '%s' [%s]\n", prop, prop_get_str(prop),
            prop_is_exported(prop) ? "True" : "False");
    }

    // Show auditor properties from the environment.
    char **envp;
    for (envp = environ; *envp; envp++) {
        if (!strncmp(*envp, ENV_PFX, sizeof(ENV_PFX) - 1)) {
            printf("ENV: '%s'\n", *envp);
        }
    }

    prop_fini();

    return 0;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
