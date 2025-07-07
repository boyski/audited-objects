#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

typedef const char *CCS;

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

// Function to parse a single CSV line and return a JSON object
cJSON*
json_parse_csv_line(CCS *columns, CCS csv_line)
{
    cJSON *json_obj;
    char *copy, *comma, *field;
    int i;

    json_obj = cJSON_CreateObject();
    for (i = 0, copy = field = strdup(csv_line); (comma = strchr(field, ',')); i++, field = comma + 1) {
	*comma = '\0';
	cJSON_AddStringToObject(json_obj, columns[i], field);
    }
    cJSON_AddStringToObject(json_obj, columns[i], field);
    free(copy);

    return json_obj;
}

// Function to convert multiple lines of CSV data to JSON array
char *
json_csv_to_array(CCS *columns, CCS csv_data)
{
    cJSON *json_array;
    char *copy, *csv_line, *nl, *json;

    json_array = cJSON_CreateArray();
    for (copy = csv_line = strdup(csv_data); (nl = strchr(csv_line, '\n')); csv_line = nl + 1) {
	cJSON *json_line;

	*nl = '\0';
	if ((json_line = json_parse_csv_line(columns, csv_line))) {
	    cJSON_AddItemToArray(json_array, json_line);
	}
    }
    free(copy);

    // Convert JSON array to string
    json = cJSON_Print(json_array);

    // Clean up
    cJSON_Delete(json_array);

    return json;
}

// Example usage
int
main(void)
{
    char *json_result;

    static CCS pa_data =
        "X,init,0.0,513467,1,513453,0,91ad7d96e56ad37b229d455243e14e67+23,f1437cce3e5bd9d60d47f56041f9ee16+15,f,?,ro2lun.0,50752,pnx,,,/bin/cut\n";

    static CCS ca_data =
	"2682633,1,2682623,syyf6h.37ivwo,0,app-lvn-wcc-7056,,,cut,.,91ad7d96e56ad37b229d455243e14e67+23,f1437cce3e5bd9d60d47f56041f9ee16+15,-,cut -d . -f 1-2\n";

    json_result = json_csv_to_array(pa_fields, pa_data);
    printf("JSON output:\n%s\n", json_result);
    free(json_result);

    json_result = json_csv_to_array(ca_fields, ca_data);
    printf("JSON output:\n%s\n", json_result);
    free(json_result);

    return 0;
}
