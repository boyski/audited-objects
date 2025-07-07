#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

typedef const char *CCS;

static CCS pa_fields[] = {"1", "2", "3", "four", "5", "6", "7", "8", "9",
    "10", "11", "12", "13", "14", "15", "16", "17", "18"};

static CCS ca_fields[] = {"one", "2", "3", "four", "5", "6", "7", "8", "9",
    "10", "11", "12", "13", "14", "15", "16", "17", "18"};

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
json_csv_to_json_array(CCS *columns, CCS csv_data)
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
    CCS pa_data =
        "X,init,0.0,513467,1,513453,0,91ad7d96e56ad37b229d455243e14e67+23,f1437cce3e5bd9d60d47f56041f9ee16+15,f,?,ro2lun.0,50752,pnx,,,/bin/cut -n -s\n"
        "Z,end,2.0,513469,3,513455,2,xyz987wvu654,mno321qrs098,h,?,ro2lun.2,50754,pnz,,,/bin/sort\n";

    CCS ca_data =
	"2682633,1,2682623,syyf6h.37ivwo,0,app-lvn-wcc-7056,,,cut,.,91ad7d96e56ad37b229d455243e14e67+23,f1437cce3e5bd9d60d47f56041f9ee16+15,-,cut -d . -f 1-2\n"
	"2682637,1,2682623,syyf6h.3b8zu8,0,app-lvn-wcc-7056,,,syslogger,.,91ad7d96e56ad37b229d455243e14e67+23,2df1778b72166f40a72df5392c1bb9f8+146,-,/tools/oss/packages/x86_64-rhel8/BRCM/default/bin/syslogger -h syslogger db936042 make 4.3 make /tools/oss/packages/x86_64-rhel8/make/4.3/bin/make\n";

    char *json_result;

    json_result = json_csv_to_json_array(pa_fields, pa_data);
    printf("JSON output:\n%s\n", json_result);
    free(json_result);

    json_result = json_csv_to_json_array(ca_fields, ca_data);
    printf("JSON output:\n%s\n", json_result);
    free(json_result);

    return 0;
}
