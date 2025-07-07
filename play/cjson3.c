#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>
#include "cJSON.h"

static struct option long_options[] = {
    {"output",      required_argument, 0, 'o'},
    {"help",        no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] FILE1 [FILE2 ...]\n", program_name);
    printf("  -o, --output FILE      Output to file instead of stdout\n");
    printf("  -h, --help             Show this help message\n");
}

cJSON* create_file_json(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return NULL;
    }
    
    cJSON *file_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(file_obj, "path", filepath);
    cJSON_AddNumberToObject(file_obj, "size", (double)st.st_size);
    cJSON_AddNumberToObject(file_obj, "timestamp", (double)st.st_mtime);
    cJSON_AddNumberToObject(file_obj, "mode", st.st_mode & 0777);
    
    return file_obj;
}

int main(int argc, char *argv[]) {
    char *output_file = NULL;
    
    int c;
    while ((c = getopt_long(argc, argv, "o:h", long_options, NULL)) != -1) {
        switch (c) {
            case 'o':
                output_file = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case '?':
	    default:
                return 1;
        }
    }
    
    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }
    
    cJSON *json_array = cJSON_CreateArray();
    
    for (int i = optind; i < argc; i++) {
        cJSON *file_json = create_file_json(argv[i]);
        if (file_json) {
            cJSON_AddItemToArray(json_array, file_json);
        }
    }
    
    char *json_string = cJSON_Print(json_array);
    
    FILE *output = output_file ? fopen(output_file, "w") : stdout;
    if (output) {
        fprintf(output, "%s\n", json_string);
        if (output_file) fclose(output);
    }
    
    free(json_string);
    cJSON_Delete(json_array);
    
    return 0;
}
