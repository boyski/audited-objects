#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"

int
main(void)
{
    // Array of string pointers
    const char *strings[] = {
        "apple",
        "banana", 
        "cherry",
        "date",
        "elderberry"
    };
    int num_strings = sizeof(strings) / sizeof(strings[0]);
    
    // Create JSON array for the strings
    cJSON *string_array = cJSON_CreateArray();
    
    // Add each string to the array
    for (int i = 0; i < num_strings; i++) {
        cJSON *string_item = cJSON_CreateString(strings[i]);
        cJSON_AddItemToArray(string_array, string_item);
    }
    
    // Convert to JSON string and print
    char *json_string = cJSON_Print(string_array);
    
    printf("Generated JSON:\n%s\n", json_string);
    
    // Clean up memory
    free(json_string);
    cJSON_Delete(string_array);
    
    return 0;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
