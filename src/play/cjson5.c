#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

static const char *pa_fields[] = {"1", "2", "3", "four", "5", "6", "7", "8", "9",
    "10", "11", "12", "13", "14", "15", "16", "17", "18"};

// Function to parse a single CSV line and return a JSON object
cJSON*
parse_csv_line(const char* csv_line)
{
    // Create JSON object for this line
    cJSON *json_line = cJSON_CreateObject();
    
    // Make a copy of the input string for parsing
    char *line_copy = strdup(csv_line);
    
    int field_count = 0;
    
    // Manually parse CSV to preserve empty fields
    char *current = line_copy;
    char *field_start = current;
    
    while (*current != '\0') {
        if (*current == ',') {
            // Found a delimiter, process the field
            *current = '\0';  // Null-terminate the field
            
            // Add all fields as strings
            cJSON_AddStringToObject(json_line, pa_fields[field_count], field_start);
            
            field_count++;
            current++;
            field_start = current;
        } else {
            current++;
        }
    }
    
    // Handle the last field (after the last comma or the only field if no commas)
    cJSON_AddStringToObject(json_line, pa_fields[field_count], field_start);
    
    // Clean up
    free(line_copy);
    
    return json_line;
}

// Function to convert multiple lines of CSV data to JSON array
char *
csv_to_json_array(const char *csv_data)
{
    // Create JSON array to hold all lines
    cJSON *json_array = cJSON_CreateArray();
    if (!json_array) return NULL;
    
    // Make a copy of the input string for parsing
    char *data_copy = strdup(csv_data);
    if (!data_copy) {
        cJSON_Delete(json_array);
        return NULL;
    }
    
    // Parse line by line
    char *current = data_copy;
    char *line_start = current;
    
    while (*current != '\0') {
        if (*current == '\n') {
            // Found a newline, process the line
            *current = '\0';  // Null-terminate the line
            
            // Skip empty lines
            if (strlen(line_start) > 0) {
                cJSON *json_line = parse_csv_line(line_start);
                if (json_line) {
                    cJSON_AddItemToArray(json_array, json_line);
                }
            }
            
            current++;
            line_start = current;
        } else {
            current++;
        }
    }
    
    // Convert JSON array to string
    char *json_string = cJSON_Print(json_array);
    
    // Clean up
    free(data_copy);
    cJSON_Delete(json_array);
    
    return json_string;
}

// Example usage
int
main(void)
{
    const char* csv_data = 
        "X,init,0.0,513467,1,513453,0,91ad7d96e56ad37b229d455243e14e67+23,f1437cce3e5bd9d60d47f56041f9ee16+15,f,?,ro2lun.0,50752,pnx,,,/bin/cut -n -s\n"
        "Z,end,2.0,513469,3,513455,2,xyz987wvu654,mno321qrs098,h,?,ro2lun.2,50754,pnz,,,/bin/sort\n";
    
    char *json_result = csv_to_json_array(csv_data);
    
    if (json_result) {
        printf("JSON output:\n%s\n", json_result);
        free(json_result);  // Don't forget to free the JSON string
    } else {
        printf("Error converting CSV to JSON\n");
    }
    
    return 0;
}
