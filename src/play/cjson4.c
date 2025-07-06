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
    char *saveptr;
    int field_count = 1;
    char field_name[20];
    
    // Tokenize the CSV line
    token = strtok_r(line_copy, ",", &saveptr);
    
    while (token != NULL) {
        // Create field name (field1, field2, etc.)
        snprintf(field_name, sizeof(field_name), "field%d", field_count);
        
        // Handle empty fields (represented as empty strings)
        if (strlen(token) == 0) {
            cJSON_AddStringToObject(json, field_name, "");
        } else {
            // Try to determine if it's a number or string
            char *endptr;
            double num_val = strtod(token, &endptr);
            
            // If the entire token was converted to a number, add as number
            if (*endptr == '\0' && token != endptr) {
		cJSON_AddNumberToObject(json, field_name, num_val);
            } else {
                // Add as string
                cJSON_AddStringToObject(json, field_name, token);
            }
        }
        
        // Get next token
        token = strtok_r(NULL, ",", &saveptr);
        field_count++;
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
