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
    
    // Create root JSON object
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        fprintf(stderr, "Failed to create JSON object\n");
        return 1;
    }
    
    // Create JSON array for the strings
    cJSON *string_array = cJSON_CreateArray();
    if (string_array == NULL) {
        fprintf(stderr, "Failed to create JSON array\n");
        cJSON_Delete(root);
        return 1;
    }
    
    // Add each string to the array
    for (int i = 0; i < num_strings; i++) {
        cJSON *string_item = cJSON_CreateString(strings[i]);
        if (string_item == NULL) {
            fprintf(stderr, "Failed to create string item\n");
            cJSON_Delete(string_array);
            cJSON_Delete(root);
            return 1;
        }
        cJSON_AddItemToArray(string_array, string_item);
    }
    
    // Add the array to the root object
    cJSON_AddItemToObject(root, "strings", string_array);
    
    // Convert to JSON string and print
    char *json_string = cJSON_Print(root);
    if (json_string == NULL) {
        fprintf(stderr, "Failed to print JSON\n");
        cJSON_Delete(root);
        return 1;
    }
    
    printf("Generated JSON:\n%s\n", json_string);
    
    // Clean up memory
    free(json_string);
    cJSON_Delete(root);
    
    return 0;
}
