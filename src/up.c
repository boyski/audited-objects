// Copyright (c) 2005-2010 David Boyce.  All rights reserved.

/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file
/// @brief Support for uploading of audit data and file contents.

#include "AO.h"

#include "CA.h"
#include "HTTP.h"
#include "PROP.h"

#include "curl/curl.h"

/// Files smaller than this are not compressed for upload, because
/// it becomes counterproductive for very small files.
#define UPLOAD_COMPRESS_MIN_SIZE		512UL

/// Initializes upload data structures.
void
up_init(void)
{
    // At the moment there is nothing to do here.
}

/// Pushes an audit buffer onto the upload stack to be sent "asap".
/// @param[in] cabuf    A buffer containing a stringified CA
void
up_load_audit(CCS cabuf)
{
    int synchronous;
    CURL *curl;
    conn_info_s *cip;
    unsigned long bufsize, zsize = 0;
    void *zdata;

    synchronous = prop_is_true(P_SYNCHRONOUS_TRANSFERS);

    if (synchronous) {
	curl = http_get_curl_handle(1);
    } else {
	curl = http_async_get_free_curl_handle();
    }

    http_add_header(curl, CONTENT_TYPE_HEADER, APPLICATION_OCTET_STREAM);

    cip = http_get_conn_info(curl);

    cip->ci_url = http_make_url(AUDIT_SERVLET_NICKNAME);

    bufsize = _tcslen(cabuf);
    if (!prop_is_true(P_UNCOMPRESSED_TRANSFERS) &&
	    (zdata = util_gzip_buffer("AUDIT",
	    (unsigned const char *)cabuf, bufsize, &zsize))) {
	http_add_header(curl, X_GZIPPED_HEADER, "1");
	cip->ci_malloced = zdata;
	bufsize = zsize;
    } else {
	cip->ci_malloced = (void *)putil_strdup(cabuf);
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, cip->ci_malloced);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, bufsize);

    // Always set the URL last since adding parameters to it might
    // lead to a realloc/move.
    curl_easy_setopt(curl, CURLOPT_URL, cip->ci_url);

    if (synchronous) {
	http_connect(curl, cip->ci_url, 0);
    } else {
	http_async_add_handle(curl);
    }
}

/// Pushes a recently-generated file onto the upload stack to be sent "asap".
/// @param[in] ps       A PathState object representing to new file
/// @param[in] logfile  A boolean indicating whether this is a log file
void
up_load_file(ps_o ps, int logfile)
{
    int synchronous;
    CURL *curl;
    conn_info_s *cip;
    CCS psbuf;
    CCS path;
    int fd;
    struct stat64 stbuf;
    unsigned long fsize;
    unsigned char *fdata;
    unsigned long zsize = 0;
    void *zdata;

    synchronous = logfile || prop_is_true(P_SYNCHRONOUS_TRANSFERS);

    if (synchronous) {
	curl = http_get_curl_handle(1);
    } else {
	curl = http_async_get_free_curl_handle();
    }

    http_add_header(curl, CONTENT_TYPE_HEADER, APPLICATION_OCTET_STREAM);

    cip = http_get_conn_info(curl);

    cip->ci_url = http_make_url(UPLOAD_SERVLET_NICKNAME);

    psbuf = ps_tostring(ps);
    http_add_header(curl, X_PATHSTATE_HEADER, psbuf);
    putil_free(psbuf);

    if (logfile) {
	http_add_header(curl, X_LOGFILE_HEADER, "1");
	http_add_param(&cip->ci_url, X_LOGFILE_HEADER, "1");
    }

    path = ps_get_abs(ps);

    if ((fd = open64(path, O_RDONLY | O_BINARY)) == -1) {
	putil_syserr(0, path);
	return;
    }

    if (fstat64(fd, &stbuf)) {
	close(fd);
	putil_syserr(0, path);
	return;
    }

    // We don't bother uploading zero-length files. 
    fsize = stbuf.st_size;
    if (!fsize) {
	close(fd);
	return;
    }

    fdata = util_map_file(path, fd, fsize);
    close(fd);

    if ((fsize > UPLOAD_COMPRESS_MIN_SIZE) &&
	    (zdata = util_gzip_buffer(path, fdata, fsize, &zsize))) {
	// If compression succeeded, we can unmap the file and
	// proceed to upload the compression buffer.
	util_unmap_file(fdata, fsize);

	http_add_header(curl, X_GZIPPED_HEADER, "1");

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, zdata);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)zsize);
	cip->ci_malloced = zdata;
    } else {
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fdata);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)fsize);
	cip->ci_mapaddr = fdata;
	cip->ci_mapsize = fsize;
    }

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, http_find_errors);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, NULL);

    // Always set the URL last since adding parameters to it might
    // lead to a realloc/move.
    curl_easy_setopt(curl, CURLOPT_URL, cip->ci_url);

    // Upload verbosity.
    pn_verbosity(ps_get_pn(ps), _T("UPLOADING"), NULL);

    if (synchronous) {
	http_connect(curl, cip->ci_url, 0);
    } else {
	http_async_add_handle(curl);
    }
}

/// Completes any unfinished uploads, then finalizes upload data structures.
void
up_fini(void)
{
    // At the moment there is nothing to do here.
}
