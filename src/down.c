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
/// @brief Support for downloading of files from the server.

#include "AO.h"

#include "DOWN.h"
#include "HTTP.h"
#include "PROP.h"
#include "PS.h"

#include "curl/curl.h"

/// @cond static
#define DISPOSITION				"Content-Disposition:"
/// @endcond static

/// Structure type used to describe an audited object being downloaded.
/// Required because callback functions allow only one data pointer.
typedef struct {
    moment_s dps_moment;	///< mod time of the object
    mode_t dps_mode;		///< Unix mode of the object
    int dps_status;		///< status of the download
} down_path_state_s;

// Parse HTTP headers looking for certain user-defined "X-*" headers
// which carry a file's metadata, while the file contents are carried
// in the response body.
static size_t
_down_hdrs_to_path_state(void *vptr, size_t size, size_t nitems, void *userp)
{
    char *hdr = (char *)vptr;
    char *word, *words;
    char *val, *end;
    down_path_state_s *dpsp;

    dpsp = (down_path_state_s *) userp;

    if (!_strnicmp(hdr, DISPOSITION, strlen(DISPOSITION))) {
	if ((words = strchr(hdr, ';') + 1)) {
	    while (isspace((int)*words)) {
		words++;
	    }
	    for (word = util_strsep(&words, " \r\n");
		 word && *word; word = util_strsep(&words, " \r\n")) {
		val = strchr(word, '"') + 1;
		end = strchr(val, '"');
		*end = '\0';
		if (!_strnicmp(word, "moment", 6)) {
		    moment_parse(&(dpsp->dps_moment), val);
		} else if (!_strnicmp(word, "mode", 4)) {
		    dpsp->dps_mode = _tcstol(val, NULL, CSV_RADIX);
		}
	    }
	}
    } else if (!_strnicmp(hdr, X_SERVER_STATUS_HEADER,
	    strlen(X_SERVER_STATUS_HEADER))) {
	dpsp->dps_status = http_parse_error_from_server(hdr);
    }

    return size * nitems;
}

/// Grab the named path state from the server and set the date,
/// mode, and ownership appropriately.
/// @param[in] ps       a PS object representing the requested file state
/// @return 0 on success
int
down_load(ps_o ps)
{
    CCS relpath, abspath;
    char *url;
    TCHAR tgtdir[PATH_MAX];
    TCHAR psbuf[PATH_MAX + 256];
    down_path_state_s dps;
    CURL *curl;
    FILE *fp;
    int rc;

    rc = 0;

    relpath = ps_get_rel(ps);
    abspath = ps_get_abs(ps);

    // In case the parent dir of the target doesn't exist, try to create it.
    putil_dirname(abspath, tgtdir);
    if (_taccess(tgtdir, F_OK)) {
	if (putil_mkdir_p(tgtdir)) {
	    putil_syserr(0, tgtdir);
	    return 1;
	}
    }

    memset(&dps, 0, sizeof(dps));

    url = http_make_url(DOWNLOAD_SERVLET_NICKNAME);
    http_add_param(&url, HTTP_PROJECT_NAME_PARAM, prop_get_str(P_PROJECT_NAME));

    if (prop_is_true(P_UNCOMPRESSED_TRANSFERS)) {
	http_add_param(&url, HTTP_UNCOMPRESSED_TRANSFERS_PARAM, HTTP_TRUE);
    }

    // The vital statistics of the PS we're requesting.
    (void)ps_toCSVString(ps, psbuf, charlen(psbuf));
    http_add_param(&url, HTTP_PS_CSV_PARAM, psbuf);

    curl = http_get_curl_handle(1);

    // Open a stream to the target and point the HTTP transaction at it.
    // We try to open it without unlinking first, in order to
    // preserve any additional hard links it may have.
    if (!(fp = fopen(abspath, _T("wb")))) {
	_tunlink(abspath);
	if (!(fp = fopen(abspath, _T("wb")))) {
	    putil_syserr(2, abspath);
	    return 1;
	}
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, _down_hdrs_to_path_state);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &dps);

    // Go get the darn file.
    rc = http_connect(curl, url, 1);

    fclose(fp);

    // There are errors which can be detected and reported within
    // http_connect() but there are also some which must be handled
    // with an out-of-band mechanism. We use a special header to
    // report server-side failures and that status will be
    // placed in the dps struct.
    if (rc == 0 && dps.dps_status != 0) {
	rc = dps.dps_status;
    }

    if (rc == 0) {
#ifndef _WIN32
	// If no mode was reported at all, let the current umask win.
	if (dps.dps_mode && chmod(abspath, dps.dps_mode)) {
	    putil_syserr(0, abspath);
	}
#endif	/*_WIN32*/
	// By default we set the mod time of the downloaded file to what
	// it was when it was uploaded, but 'now' can also be requested.
	// If no timestamp was sent by the server, 'now' it is.
	if (prop_is_true(P_ORIGINAL_DATESTAMP) &&
		moment_is_set(dps.dps_moment)) {
	    if (moment_set_mtime(&(dps.dps_moment), abspath)) {
		putil_syserr(0, abspath);
	    }
	}
    } else {
	// If the download failed, we should remove the stub in order
	// to not confuse a timestamp-based tool like make.
	// This could potentially break a hard link but not removing it
	// would be worse IMHO.
	_tunlink(abspath);
    }

    return rc;
}
