/*
 * Build: gcc -g -W -Wall -D_REENTRANT -I ../../OPS/include -o multi-post multi-post.c -L../../OPS/SunOS_i386/lib -lcurl -lz -lsocket -lnsl -lrt
 *
 * Run:   ./multi-post *.*
 *
 */

#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <curl/curl.h>

#define OCTET_STREAM                      "application/octet-stream"

int
main(int argc, char *argv[])
{
    int rc = 0;
    CURL *curl1, *curl2;
    CURLM *multi_handle;
    int still_running;
    char *file1, *file2;
    struct curl_httppost *formpost1 = NULL;
    struct curl_httppost *formpost2 = NULL;
    struct curl_httppost *lastptr1 = NULL;
    struct curl_httppost *lastptr2 = NULL;
    struct curl_slist *headerlist = NULL;

    if (argc < 2) {
	file1 = "/etc/passwd";
    } else {
	file1 = argv[1];
	if (argc < 3) {
	    file2 = "/etc/group";
	} else {
	    file2 = argv[2];
	}
    }

    curl_formadd(&formpost1, &lastptr1,
		 CURLFORM_CONTENTTYPE, OCTET_STREAM,
		 CURLFORM_COPYNAME, "file1",
		 CURLFORM_COPYCONTENTS, "FILE1",
		 CURLFORM_CONTENTSLENGTH, (long)5,
		 CURLFORM_END);

    curl_formadd(&formpost2, &lastptr2,
		 CURLFORM_CONTENTTYPE, OCTET_STREAM,
		 CURLFORM_COPYNAME, "file2",
		 CURLFORM_COPYCONTENTS, "FILE2",
		 CURLFORM_CONTENTSLENGTH, (long)5,
		 CURLFORM_END);

    /*
     * Initalize custom header list (stating that Expect: 100-continue
     * is not wanted.
     */
    headerlist = curl_slist_append(headerlist, "Expect:");

    curl1 = curl_easy_init();
    curl_easy_setopt(curl1, CURLOPT_URL, "http://papi:7000/AO/LAB");
    curl_easy_setopt(curl1, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl1, CURLOPT_HTTPHEADER, headerlist);
    curl_easy_setopt(curl1, CURLOPT_HTTPPOST, formpost1);

    curl2 = curl_easy_init();
    curl_easy_setopt(curl2, CURLOPT_URL, "http://papi:7000/AO/LAB");
    curl_easy_setopt(curl2, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl2, CURLOPT_HTTPHEADER, headerlist);
    curl_easy_setopt(curl2, CURLOPT_HTTPPOST, formpost2);

    multi_handle = curl_multi_init();
    curl_multi_add_handle(multi_handle, curl1);
    curl_multi_add_handle(multi_handle, curl2);

    while (CURLM_CALL_MULTI_PERFORM ==
	   curl_multi_perform(multi_handle, &still_running));

    while (still_running) {
	struct timeval timeout;
	int sret;

	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;
	int maxfd;

	FD_ZERO(&fdread);
	FD_ZERO(&fdwrite);
	FD_ZERO(&fdexcep);

	/* set a suitable timeout to play around with */
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	/* get file descriptors from the transfers */
	curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
	if (maxfd == -1) {
	    rc = 2;
	    break;
	}

	sret = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);

	switch (sret) {
	    case -1:
		/* select error */
		break;
	    case 0:
		printf("timeout!\n");
	    default:
		/* timeout or readable/writable sockets */
		printf("perform!\n");
		while (CURLM_CALL_MULTI_PERFORM ==
		       curl_multi_perform(multi_handle, &still_running));
		printf("running: %d!\n", still_running);
		break;
	}
    }

    (void)curl_multi_remove_handle(multi_handle, curl1);
    curl_easy_cleanup(curl1);

    (void)curl_multi_remove_handle(multi_handle, curl2);
    curl_easy_cleanup(curl2);

    curl_multi_cleanup(multi_handle);

    curl_formfree(formpost1);
    curl_formfree(formpost2);

    curl_slist_free_all(headerlist);

    return rc;
}
