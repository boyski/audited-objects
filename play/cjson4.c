#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

// Function to split CSV line and convert to JSON
char *
csv_to_json(const char* csv_line)
{
    if (!csv_line) return NULL;

    // Create JSON object
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;

    // Make a copy of the input string for tokenization
    char *line_copy = strdup(csv_line);
    if (!line_copy) {
        cJSON_Delete(json);
        return NULL;
    }

    char *token;
    char *fldptr;
    int i = 1;
    char field_name[20];

    for (i = 1, token = strtok_r(line_copy, ",", &fldptr); token; i++, token = strtok_r(NULL, ",", &fldptr)) {
        // Create field name (field1, field2, etc.)
        snprintf(field_name, sizeof(field_name), "field%d", i);

	cJSON_AddStringToObject(json, field_name, *token ? token : "");
    }

    // Convert JSON object to string
    char *json_string = cJSON_Print(json);

    // Clean up
    free(line_copy);
    cJSON_Delete(json);

    return json_string;
}

// Example usage
int
main(void)
{
    const char* csv_line = "X,init,0.0,513467,1,513453,0,91ad7d96e56ad37b229d455243e14e67+23,f1437cce3e5bd9d60d47f56041f9ee16+15,f,?,ro2lun.0,50752,pnx,,,/bin/cut";

    char* json_result = csv_to_json(csv_line);

    if (json_result) {
        printf("JSON output:\n%s\n", json_result);
        free(json_result);  // Don't forget to free the JSON string
    } else {
        printf("Error converting CSV to JSON\n");
    }

    return 0;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
