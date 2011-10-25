// gcc -W -Wall -O -o urlencode urlencode.c -lcurl

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "curl/curl.h"

int
main(int argc, char *argv[])
{
    char orig[8192];
    char *changed;
    ssize_t n;
    int decode = strstr(argv[0], "decode") != NULL;
    argc = argc;

    while ((n = read(0, orig, sizeof(orig)))) {
	changed = decode ? curl_unescape(orig, n) : curl_escape(orig, n);
	if (! changed) {
	    perror("curl_escape");
	    exit(2);
	}
	write(1, changed, strlen(changed));
	free(changed);
    }

    exit(0);
}
