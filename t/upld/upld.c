// Compile line:
// gcc -o upld -Wall $(getconf LFS_CFLAGS) upld.c $(curl-config --libs)

#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

int
main(int argc, char *argv[])
{
    CURL *curl;
    CURLcode res;
    CURLFORMcode cfrc;
    char errbuf[CURL_ERROR_SIZE] = "";
    char *path;
    char *url;

    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;

    if (argc != 3) {
	fprintf(stderr, "Usage: %s <path> <upload-url>\n", argv[0]);
	exit(2);
    }

    path = argv[1];
    url  = argv[2];

    curl_global_init(CURL_GLOBAL_ALL);

    cfrc = curl_formadd(&formpost, &lastptr,
	    CURLFORM_CONTENTTYPE, "application/binary",
	    CURLFORM_COPYNAME, basename(path),
	    CURLFORM_FILE, path, CURLFORM_END);

    curl = curl_easy_init();

    if (curl) {
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (errbuf[0]) {
	    fprintf(stderr, "Error: %s\n", errbuf);
	    exit(2);
	}
    }

    curl_formfree(formpost);
    curl_global_cleanup();

    return 0;
}
