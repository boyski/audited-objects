// Copyright (c) 2002-2010 David Boyce.  All rights reserved.

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
/// @brief Support for HTTP conversations with the server using libcurl.
/// Note that although we try to encapsulate ancillary technologies
/// such as HTTP, regular expressions, compression, hashing, etc.
/// to make it easier to swap out one such library for another,
/// full abstraction is not always possible. That applies here;
/// although we try to put as much of the libcurl-related code as
/// possible in http.c, other modules still do libcurl-specific things.
/// Fortunately I like libcurl a lot and see no reason to think
/// about replacing it.

#include <locale.h>

#include "AO.h"
#include "PS.h"

#include "curl/curl.h"

#include "bsd_getopt.h"

#include "HTTP.h"
#include "PROP.h"

/// @cond static
#define ACTION_SERVLET_PREFIX			"action"
#define LABEL_SERVLET_NICKNAME			"label"
#define NAMESTATE_SERVLET_NICKNAME		"namestate"
#define FIND_SERVLET_NICKNAME			"find"

#define ACTION_ARGS_PARAM			"ARGS"
/// @endcond static

/// The number of transfers (typically uploads) allowed at once.
#define SIMULTANEOUS_TRANSFER_MAX		50

static CURLM *MultiHandle;

static FILE *Output_FP;

static CURL *DefaultHandle;
static char *SessionCookie;

static double NameLookup_Time;
static double Connect_Time;
static double AppConnect_Time;
static double PreTransfer_Time;
static double StartTransfer_Time;
static double Cumulative_Time;

// Internal service routine.
static void
_http_dump_unescaped(char *escaped, int len, FILE *strm)
{
    char *unescaped;

    if ((unescaped = curl_unescape(escaped, len))) {
	fputs(unescaped, strm);
	curl_free(unescaped);
	fputc('\n', strm);
    }
}

// Internal service routine. Used for certain kinds of verbosity.
static int
_debug_curl(CURL *curl, curl_infotype type, char *buf, size_t len, void *userp)
{
    FILE *vstrm;

    // Stolen from Curl_debug.
    static const char *const s_infotype[CURLINFO_END] = {
	"* ", "< ", "> ", "{ ", "} "
    };

    UNUSED(curl);

    if (!vb_bitmatch(VB_HTTP) && !vb_bitmatch(VB_URL)) {
	return 0;
    }

    vstrm = (FILE *)userp;	// the stdio stream for verbose output

    if (vb_bitmatch(VB_URL)) {
	if (type == CURLINFO_HEADER_OUT) {
	    char *cr;

	    fwrite(s_infotype[type], 2, 1, vstrm);
	    if ((cr = strchr(buf, '\r'))) {
		len = cr - buf;
	    }
	    _http_dump_unescaped(buf, (int)len, vstrm);
	}
    }

    if (vb_bitmatch(VB_HTTP)) {
	static int count;

	switch (type) {
	    case CURLINFO_HEADER_OUT:
		// Decrement the count by two to skip the
		// CRLF pair which delimits headers from body.
		if (len > 2) {
		    len -= 2;
		}
		// fall through to regular mode ...
	    case CURLINFO_TEXT:
		// Put a newline between each http request/response cycle.
		if (count++ && !strncmp("About to ", buf, 9)) {
		    fputc('\n', vstrm);
		}
		// fall through ...
	    case CURLINFO_HEADER_IN:
		fwrite(s_infotype[type], 2, 1, vstrm);
		fwrite(buf, len, 1, vstrm);
		break;
	    default:
		break;
	}
    }

    return 0;
}

// HTTP data is not separated into stdout and stderr streams; it's
// one big body, broken by libcurl into a sequence of equal-sized
// buffers. When textual data is sent via HTTP lines may break
// across buffer boundaries, so we need some logic to re-line-orient
// it and then divert those lines which look like error messages
// to stderr. We presume that such lines should cause the return
// code to be nonzero, so the data pointer points to an integer
// in which we can set a return code.
static size_t
_http_stream_body(void *buffer, size_t size, size_t nitems, void *rc_p)
{
    static CS leftover;
    CS body, curr;
    size_t buflen;
    int *rcptr;
    CCS line;

    buflen = size * nitems;

    // If non-null this points to an integer which receives a return code.
    rcptr = (int *)rc_p;

    if (leftover && *leftover) {
	size_t len;

	len = _tcslen(leftover);
	body = curr = (CS)putil_malloc(buflen + len + 1);
	memcpy(body, leftover, len);
	memcpy(body + len, buffer, buflen);
	body[len + buflen] = '\0';
	putil_free(leftover);
    } else {
	body = curr = (CS)putil_malloc(buflen + 1);
	memcpy(body, buffer, buflen);
	body[buflen] = '\0';
    }

    if (*curr == '\n') {
	curr++;
    }

    // This code tries to be forgiving of both CRLF and LF terminated
    // lines, even though the HTTP spec calls for CRLF only.
    for (leftover = curr;
	 (line = util_strsep(&curr, "\r\n")) && curr; leftover = curr) {

	if (!_tcsncmp(line, HTTP_NOTE, charlen(HTTP_NOTE) - 1)) {
	    if (vb_bitmatch(VB_STD)) {
		_fputts(prop_get_app(), stderr);
		_fputts(_T(": "), stderr);
		_fputts(line + charlen(HTTP_NOTE) - 1, stderr);
		_fputts(_T("\n"), stderr);
	    }
	} else if (!_tcsncmp(line, HTTP_ERROR, charlen(HTTP_ERROR) - 1)) {
	    if (rcptr && !*rcptr) {
		*rcptr = 2;
	    }
	    putil_error(_T("%s"), line + charlen(HTTP_ERROR) - 1);
	} else if (!_tcsncmp(line, HTTP_WARNING, charlen(HTTP_WARNING) - 1)) {
	    putil_warn(_T("%s"), line + charlen(HTTP_WARNING) - 1);
	} else {
	    FILE *fp;

	    fp = Output_FP ? Output_FP : stdout;
	    _fputts(line, fp);
	    _fputts(_T("\n"), fp);
	}

	if (*curr == '\n') {
	    curr++;
	}
    }

    if (leftover != body) {
	if (leftover && *leftover) {
	    leftover = putil_strdup(leftover);
	} else {
	    leftover = NULL;
	}
	putil_free(body);
    }

    // Always return this or else the transfer will abort.
    return buflen;
}

// Internal service routine.
static void
_http_print_time_stats(CURL *curl, CCS url)
{
    double sample_time, total_time, t1 = 0;
    double upload_size, download_size;
    double size = 0.0, speed = 0.0;
    char servlet[PATH_MAX] = "???";
    char *b, *e;

    if ((e = strchr(url, '?')) || (e = strchr(url, '&'))) {
	for (b = e - 1; b > url && *b != '/'; b--);
	strncpy(servlet, b + 1, e - b - 1);
	servlet[e - b] = '\0';
    } else if ((e = strrchr(url, '/'))) {
	strcpy(servlet, e + 1);
    }

    if ((curl_easy_getinfo(curl,
		   CURLINFO_NAMELOOKUP_TIME, &sample_time) == CURLE_OK)) {
	NameLookup_Time += sample_time;
	t1 += sample_time;
    }
    if ((curl_easy_getinfo(curl,
		   CURLINFO_CONNECT_TIME, &sample_time) == CURLE_OK)) {
	Connect_Time += sample_time;
	t1 += sample_time;
    }
#if defined(CURLINFO_APPCONNECT_TIME)
    if ((curl_easy_getinfo(curl,
		   CURLINFO_APPCONNECT_TIME, &sample_time) == CURLE_OK)) {
	AppConnect_Time += sample_time;
	t1 += sample_time;
    }
#endif
    if ((curl_easy_getinfo(curl,
		   CURLINFO_PRETRANSFER_TIME, &sample_time) == CURLE_OK)) {
	PreTransfer_Time += sample_time;
	t1 += sample_time;
    }
    if ((curl_easy_getinfo(curl,
		   CURLINFO_PRETRANSFER_TIME, &sample_time) == CURLE_OK)) {
	StartTransfer_Time += sample_time;
	t1 += sample_time;
    }

    if ((curl_easy_getinfo(curl,
		       CURLINFO_TOTAL_TIME, &total_time) == CURLE_OK)) {
	Cumulative_Time += total_time;
    }

    if ((curl_easy_getinfo(curl,
			   CURLINFO_SIZE_UPLOAD, &upload_size) == CURLE_OK)
	&& (curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &download_size)
	    == CURLE_OK)) {
	if (upload_size == 0.0 && download_size == 0.0) {
	    size = speed = 0.0;
	} else if (upload_size > download_size) {
	    size = upload_size;
	    curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD, &speed);
	} else {
	    size = download_size;
	    curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &speed);
	}
    }

    vb_printfA(VB_TIME, "HTTP %s: time=%.2fs[%.2f] size=%.0f speed=%.0fbps",
	       servlet, total_time, t1, size, speed);
}

/// Initializes all HTTP-related data structures.
void
http_init(void)
{
    vb_printf(VB_CURL, "Libcurl init");
    if (curl_global_init(CURL_GLOBAL_ALL)) {
	putil_die(_T("internal error at %s:%d"), __FILE__, __LINE__);
    }
    if (!prop_is_true(P_SYNCHRONOUS_TRANSFERS)) {
	if (!(MultiHandle = curl_multi_init())) {
	    putil_die(_T("internal error at %s:%d"), __FILE__, __LINE__);
	}
    }
}

/// Finalizes HTTP-related data structures.
void
http_fini(void)
{
    conn_info_s *cip;

    vb_printfA(VB_TIME, "HTTP Cumulative: time=%.2fs", Cumulative_Time);
    vb_printfA(VB_TIME, "HTTP NameLookup: time=%.2fs", NameLookup_Time);
    vb_printfA(VB_TIME, "HTTP Connect: time=%.2fs", Connect_Time);
    vb_printfA(VB_TIME, "HTTP AppConnect: time=%.2fs", AppConnect_Time);
    vb_printfA(VB_TIME, "HTTP PreTransfer: time=%.2fs", PreTransfer_Time);
    vb_printfA(VB_TIME, "HTTP StartTransfer: time=%.2fs", StartTransfer_Time);
    vb_printf(VB_CURL, "Final libcurl cleanup");

    if (DefaultHandle) {
	cip = http_get_conn_info(DefaultHandle);
	putil_free(cip);
	curl_easy_cleanup(DefaultHandle);
    }

    putil_free(SessionCookie);

    if (MultiHandle) {
	CURLMsg *curlmsg;
	int msgcnt = 0;

	// Finalize all easy handles contained within the multi handle.
	while ((curlmsg = curl_multi_info_read(MultiHandle, &msgcnt))) {
	    if (curlmsg->msg == CURLMSG_DONE) {
		CURL *curl;

		curl = curlmsg->easy_handle;
		curl_multi_remove_handle(MultiHandle, curlmsg->easy_handle);
		cip = http_get_conn_info(curl);
		http_clear_conn_info(curl);
		putil_free(cip);
		curl_easy_cleanup(curl);
	    }
	}

	// And finalize the multi handle itself.
	curl_multi_cleanup(MultiHandle);
	MultiHandle = NULL;
    }

    curl_global_cleanup();
}

/// Retrieves our "extra data" structure from the libcurl easy handle.
/// @param[in] curl     the curl easy handle
/// @return a pointer to the connection info struct
conn_info_s *
http_get_conn_info(CURL *curl)
{
    conn_info_s *cip;

    curl_easy_getinfo(curl, CURLINFO_PRIVATE, (char **)&cip);
    return cip;
}

/// Releases all data held by the "extra data" structure associated with
/// the specified handle. Also zeroes out the structure when done, but
/// does NOT free the struct itself.
/// @param[in] curl     the curl easy handle
void
http_clear_conn_info(CURL *curl)
{
    conn_info_s *cip;

    if ((cip = http_get_conn_info(curl))) {
	putil_free(cip->ci_url);
	putil_free(cip->ci_malloced);
	putil_free(cip->ci_verbosity);
	util_unmap_file(cip->ci_mapaddr, cip->ci_mapsize);
	curl_slist_free_all(cip->ci_extra_headers);
	memset(cip, 0, sizeof(*cip));
    }

}

/// Returns an appropriate libcurl handle for a server conversation.
/// Can return either the single, pre-existing, ready-to-go "default"
/// handle or generate a brand new one. Typically the default handle
/// should be used for synchronous connections and new handles used
/// for async work. If a new handle is requested, the caller must take
/// responsibility for cleaning it up afterward.
/// @param[in] reuse  if true, use the standard synchronous handle
/// @return a curl easy handle, possibly already connected
CURL *
http_get_curl_handle(int reuse)
{
    conn_info_s *cip;
    CURL *curl;
    char *al;

    // The default value of CURLOPT_WRITEDATA is actually 'stderr',
    // but CURLOPT_WRITEFUNCTION is set to a function that knows
    // how to deal with NULL. Therefore, if WRITEFUNCTION is set
    // to something else, CURLOPT_WRITEDATA *MUST* be reset also.

    if (!SessionCookie) {
	const char *sess;

	if ((sess = prop_get_str(P_SESSIONID))) {
	    (void)asprintf(&SessionCookie, "JSESSIONID=%s", sess);
	}
    }

    if (reuse && DefaultHandle) {
	curl = DefaultHandle;
	cip = http_get_conn_info(curl);
	http_clear_conn_info(curl);
	putil_free(cip);
	curl_easy_reset(curl);
	vb_printf(VB_CURL, _T("Curl handle %p reset"), curl);
    } else {
	if (!(curl = curl_easy_init())) {
	    putil_die(_T("internal error at %s:%d"), __FILE__, __LINE__);
	}
	if (reuse) {
	    DefaultHandle = curl;
	}
	vb_printf(VB_CURL, _T("New handle %p"), curl);
    }

    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, _debug_curl);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, vb_get_stream());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _http_stream_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L);

    cip = putil_calloc(1, sizeof(*cip));
    curl_easy_setopt(curl, CURLOPT_PRIVATE, cip);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, cip->ci_errbuf);

    // Set the standard user-agent header.
    {
	struct utsname sysdata;
	char *user_agent;

	if (putil_uname(&sysdata) == -1) {
	    putil_syserr(2, _T("uname"));
	}

	if (asprintf(&user_agent, "%s %s (%s %s %s)",
				  APPLICATION_NAME, APPLICATION_VERSION,
				  sysdata.sysname, sysdata.release,
				  sysdata.machine) < 0) {
	    putil_syserr(2, NULL);
	}

	// Could use CURLOPT_USERAGENT but this infrastructure is
	// already in place and knows how to free the data when done.
	http_add_header(curl, "User-Agent", user_agent);
	putil_free(user_agent);
    }

    // For debugging: mention the address of the handle as a header so
    // we can see that they're being used efficiently. Could be removed.
    {
	char ptrbuf[32];

	(void)snprintf(ptrbuf, sizeof(ptrbuf), "%p", curl);
	http_add_header(curl, "X-Curl-Handle", ptrbuf);
    }

    // Add an Accept-Language header providing the client locale.
    // This breaks the rules somewhat because there's a difference
    // between language and locale but it's OK for a closed app.
    // Anyway, the servlet API seems to assume you'll do this
    // because the getLocale() method looks at Accept-Language.
    if ((al = setlocale(LC_ALL, ""))) {
	char *e;

	if ((e = strchr(al, '.'))) {
	    *e = '\0';
	}
	http_add_header(curl, "Accept-Language", al);
    }

    // Shut off Expect: 100-Continue behavior. When client and
    // server know each other intimately it's unneeded complexity.
    http_add_header(curl, "Expect", "");

    // Pass the session ID cookie back to the server if present.
    curl_easy_setopt(curl, CURLOPT_COOKIE, SessionCookie);

    // Tell the server we're prepared to handle gzip- or deflate-
    // formatted as well as "identity" (unencoded) response bodies.
    curl_easy_setopt(curl, CURLOPT_ENCODING, _T(""));

    // Useful for stress testing ...
    // curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 16L);

    return curl;
}

// Internal service routine.
static int
_http_check_connect_result(CURL *curl, CURLcode res)
{
    int rc = 0;
    long http_code = -1;
    conn_info_s *cip;

    cip = http_get_conn_info(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (res != CURLE_OK || http_code != 200) {
	putil_error("%s [%u/%ld]", cip->ci_errbuf, res, http_code);
	rc = 2;
    }

    // Optional verbosity mode for reporting HTTP stats.
    if (vb_bitmatch(VB_TIME)) {
	_http_print_time_stats(curl, cip->ci_url);
    }

    return rc;
}

/// Adds a curl easy handle to the multi stack. Just a wrapper
/// over curl_multi_add_handle.
/// @param[in] curl     the curl easy handle
void
http_async_add_handle(CURL *curl)
{
    curl_multi_add_handle(MultiHandle, curl);
}

/// Looks through the supplied multi-handle for an easy handle which
/// has finished its transfer and is ready to be used again. If no
/// free handle is found, a new one is generated. This function
/// is guaranteed to return a usable handle in either case.
/// @return a curl easy handle ready for use
CURL *
http_async_get_free_curl_handle(void)
{
    CURLMsg *curlmsg;
    int msgcnt = 0;
    CURL *curl = NULL;
    static int in_use;

    while (!curl && (curlmsg = curl_multi_info_read(MultiHandle, &msgcnt))) {
	if (curlmsg->msg == CURLMSG_DONE) {
	    int rc;

	    curl = curlmsg->easy_handle;

	    rc = _http_check_connect_result(curl, curlmsg->data.result);
	    if (rc && prop_is_true(P_STRICT_UPLOAD)) {
		putil_die(_T("unable to get libcurl handle (%d)"), rc);
	    }

	    curl_multi_remove_handle(MultiHandle, curl);

	    // Free any extra metadata allocated for the handle.
	    http_clear_conn_info(curl);
	}
    }

    if (curl) {
	vb_printf(VB_UP, _T("Reusing handle %p"), curl);
    } else {
	curl = http_get_curl_handle(0);
	in_use++;
	vb_printf(VB_UP, _T("Issuing handle %p (in use=%d)"), curl, in_use);

	// When the number of simultaneous transfers begins to get
	// ridiculous, pump for a while until the waters recede.
	if (in_use >= SIMULTANEOUS_TRANSFER_MAX) {
	    http_async_transfer(in_use / 2);
	}
    }

    return curl;
}

/// Pumps as much data as possible to the server without blocking.
/// Keep pumping until the number of active transfers is less than
/// limit. A limit of 0 means "no limit".
/// @param[in] limit     the maximum number of active transfers plus one
void
http_async_transfer(int limit)
{
    if (MultiHandle) {
	CURLMcode sc;
	int still_running;

	while(1) {
	    sc = curl_multi_perform(MultiHandle, &still_running);
	    if (sc == CURLM_CALL_MULTI_PERFORM) {
		// Supposedly this test became obsolete in libcurl 7.20.0,
		// but in order to allow AO to be built against older
		// (bundled) libcurls we will leave it in for a while.
		continue;
	    } else if (still_running == 0) {
		break;
	    } else if (limit == 0) {
		break;
	    } else if (still_running < limit) {
		break;
	    } else {
		vb_printf(VB_OFF, _T("Scaling down from %d to %d handles\n"),
		    still_running, limit);
	    }
	}
    }
}

/// Initiates a synchronous round-trip communication with the server.
/// @param[in] curl     a curl easy handle to be used for the connection
/// @param[in] url      the URL to connect to
/// @param[in] loud     boolean - requests extra verbosity
/// @return 0 on success
int
http_connect(CURL *curl, char *url, int loud)
{
    conn_info_s *cip;
    CURLcode res;
    long http_code = -1;
    int rc = 0;

    UNUSED(loud);

    cip = http_get_conn_info(curl);

    // Make sure there's no leftover stuff in the error buffer.
    cip->ci_errbuf[0] = '\0';

    // Always provide the client version.
    http_add_param(&url, HTTP_CLIENT_VERSION_PARAM, _T(APPLICATION_VERSION));

    // Always set the URL last since adding parameters to it might
    // lead to a realloc/move.
    curl_easy_setopt(curl, CURLOPT_URL, cip->ci_url = url);

    //////////////////////////////////////////////////////////////////
    // THE ACTUAL CONNECTION!!!
    //////////////////////////////////////////////////////////////////

    res = curl_easy_perform(curl);

    // Even if the connection was made successfully, we may consider
    // it a failure based on the HTTP status code.
    if (res == CURLE_OK) {
	res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code == 200) {
	    rc = 0;
	} else {
	    rc = 2;
	}
    } else {
	rc = 2;
    }

    // Error handling.
    if (rc) {
	if (*cip->ci_errbuf) {
	    putil_error("%s [%s]", cip->ci_errbuf, prop_get_strA(P_SERVER));
	} else {
	    putil_error("HTTP code %ld", http_code);
	}
    } else {
	// Optional verbosity mode for reporting HTTP stats.
	if (vb_bitmatch(VB_TIME)) {
	    _http_print_time_stats(curl, url);
	}
    }

    return rc;
}

// Internal service routine.
static char *
_http_url_encode(CCS value)
{
#if defined(_UNICODE)
    char valbuf[4096];

    if ((wcstombs(valbuf, value, sizeof(valbuf)) == (size_t)-1)) {
	putil_syserr(2, value);
    }
    return curl_easy_escape(NULL, valbuf, 0);
#else				/*!_UNICODE */
    return curl_easy_escape(NULL, value, 0);
#endif				/*!_UNICODE */
}

/// Adds the specified header to the handle's set. Added headers will
/// be used for all subsequent connections. This data is automatically
/// freed by handle cleanup.
/// @param[in] curl     a curl easy handle to be used for the connection
/// @param[in] header   a string representing the header name
/// @param[in] value    a string representing the header value
void
http_add_header(CURL *curl, const char *header, const char *value)
{
    conn_info_s *cip;

    if ((cip = http_get_conn_info(curl))) {
	char *hdrstr;

	if (asprintf(&hdrstr, "%s: %s", header, value) > 0) {
	    if (!(cip->ci_extra_headers =
		    curl_slist_append(cip->ci_extra_headers, hdrstr))) {
		putil_die(_T("internal error at %s:%d"), __FILE__, __LINE__);
	    }
	    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, cip->ci_extra_headers);
	    putil_free(hdrstr);
	}
    }
}

/// Adds the specified parameter to the URL's query string.
/// @param[out] uptr    a pointer to the URL which will be realloc-ed to fit
/// @param[in] param    a string representing a parameter name
/// @param[in] value    a string representing a parameter value
void
http_add_param(char **uptr, CCS param, CCS value)
{
    char *ptemp, *vtemp;
    char sep;
    size_t olen, nlen;

    // In HTTP, not setting a param is the same as setting it to nothing.
    // This allows us to pass in property values which may not be set.
    if (!value || !*value) {
	return;
    }

    ptemp = _http_url_encode(param);
    vtemp = _http_url_encode(value);

    // Determine lengths of old (passed in) and new URLs.
    olen = strlen(*uptr);
    nlen = 1 + strlen(ptemp) + 1 + strlen(vtemp);

    // There's only one literal question mark allowed in a URL and
    // it begins the parameters.
    if (!strchr(*uptr, '?')) {
	sep = '?';
    } else {
	sep = '&';
    }

    // Stretch the passed URL to fit the new param and copy it in.
    *uptr = (char *)putil_realloc(*uptr, olen + nlen + 1);
    (void)snprintf(*uptr + olen, nlen + 1, "%c%s=%s", sep, ptemp, vtemp);

    curl_free(ptemp);
    curl_free(vtemp);

    return;
}

// Creates a basic URL with the appropriate server, port, context, etc.
// @param[in] pfx      an optional string to insert after the webapp context
// @param[in] cmd      a string which maps to an action via web.xml
// @return an allocated URL string
static char *
_http_make_url(CCS pfx, const char *cmd)
{
    char *url;
    const char *svr, *ctx;
    int rc;

    // Ensure that we have a server to talk to.
    svr = prop_get_strA(P_SERVER);
    if (!svr || !*svr) {
	putil_die("missing Server property");
    }

    // The 'context' part of the URL.
    if (!(ctx = prop_get_strA(P_SERVER_CONTEXT))) {
	ctx = prop_get_app();
    }

    if (pfx) {
	rc = asprintf(&url, "http://%s/%s/%s/%s", svr, ctx, pfx, cmd);
    } else {
	rc = asprintf(&url, "http://%s/%s/%s", svr, ctx, cmd);
    }

    if (rc < 0 || !url) {
	putil_syserr(2, _T("asprintf"));
    }

    return url;
}

/// Creates a basic URL with the appropriate server, port, context, etc.
/// @param[in] cmd      a string which maps to an action via web.xml
/// @return an allocated URL string
char *
http_make_url(const char *cmd)
{
    return _http_make_url(NULL, cmd);
}

/// Removes newline characters (CRLF since that is the HTTP standard)
/// from the supplied string.
/// @param[in,out] buf          the string buffer to chomp
/// @return the passed-in string buffer, chomped
char *
http_chomp(char *buf)
{
    char *ep;

    ep = strchr(buf, '\0');

    if (*--ep == '\n') {
	*ep = '\0';
    }
    if (*--ep == '\r') {
	*ep = '\0';
    }

    return buf;
}

/// Parses the special "X-ReturnCode" header to get the return code
/// and prints the associated error message if present.
/// @param[in] hdr      the HTTP header to be parsed 
/// @return the error code supplied by the server
int
http_parse_error_from_server(char *hdr)
{
    char *ptr = NULL;
    int rc = 0;

    // Expected format is "X-ReturnCode: <rc> <error message>"
    // but the presence of the header at all means there's some
    // kind of problem, so even a badly formatted version should
    // result in an error message.
    http_chomp(hdr);
    if ((ptr = strchr(hdr, ':'))) {
	ptr++;
	while (isspace((int)*ptr)) {
	    ptr++;
	}
	if (!(rc = atoi(ptr))) {
	    rc = 2;
	}
	while (isdigit((int)*ptr)) {
	    ptr++;
	}
	while (isspace((int)*ptr)) {
	    ptr++;
	}
	if (*ptr) {
	    putil_die(_T("%s"), ptr);
	}
    } else {
	rc = 3;
	putil_error(_T("%s"), hdr);
    }

    return rc;
}

/// Runs through HTTP headers looking for our custom header
/// which indicates a server problem and sets a return code.
/// @param[in] vptr     the HTTP header being processed
/// @param[in] size     unused but required by libcurl contract
/// @param[in] nitems   unused but required by libcurl contract
/// @param[in] userp    unused but required by libcurl contract
/// @return the number of bytes processed per libcurl policy
size_t
http_find_errors(void *vptr, size_t size, size_t nitems, void *userp)
{
    char *hdr = (char *)vptr;

    UNUSED(userp);

    if (!_strnicmp(hdr, X_SERVER_STATUS_HEADER,
	    strlen(X_SERVER_STATUS_HEADER))) {
	int rc;

	rc = http_parse_error_from_server(hdr);
	exit(rc);
    }

    return size * nitems;
}

/// Lets the server "push" initial property values to the client side.
/// This is a libcurl callback function called once per HTTP header.
/// Used for the initial connection establishing a session.
/// When the custom header "X-Set-Property" arrives, its
/// RHS is treated as a property assignment.
/// Set-Cookie sets a cookie which is sent back to keep all
/// subsequent transactions in the same HTTP session.
/// @param[in] vptr     the HTTP header being processed
/// @param[in] size     unused but required by libcurl contract
/// @param[in] nitems   unused but required by libcurl contract
/// @param[in] userp    unused but required by libcurl contract
/// @return the number of bytes processed per libcurl policy
size_t
http_parse_startup_headers(void *vptr, size_t size, size_t nitems, void *userp)
{
    const char *xprops = X_SET_PROPERTY_HEADER;
    const char *cookie = "Set-Cookie:";
    const char *sessid = "JSESSIONID=";
    char *hdr;

    UNUSED(userp);

    hdr = (char *)vptr;

    if (!strncmp(hdr, xprops, strlen(xprops))) {
	char *name, *val;
	prop_e prop;

	http_chomp(hdr);

	name = strchr(hdr, ':') + 1;
	while (isspace((int)*name)) {
	    name++;
	}

	val = strchr(name, '=');
	*val++ = '\0';

#if defined(_UNICODE)
	wchar_t wname[1024], wval[4096];

	if (mbstowcs(wname, name, charlen(wname)) == (size_t)-1) {
	    putil_die(_T("mbstowcs()"));
	}

	if (mbstowcs(wval, val, charlen(wval)) == (size_t)-1) {
	    putil_die(_T("mbstowcs()"));
	}

	if ((prop = prop_from_name(wname)) != P_BADPROP) {
	    prop_put_str(prop, wval);
	} else {
	    putil_int(_T("property %s"), wname);
	}
#else				/*!_UNICODE */
	if ((prop = prop_from_name(name)) != P_BADPROP) {
	    prop_put_str(prop, val);
	} else {
	    putil_int(_T("property %s"), name);
	}
#endif				/*!_UNICODE */
    } else if (!_strnicmp(hdr, cookie, strlen(cookie))) {
	char *id;

	http_chomp(hdr);

	if ((id = strstr(hdr, sessid))) {
	    char *semi;

	    id = strchr(id, '=') + 1;
	    semi = strchr(id, ';');
	    *semi = '\0';
	    prop_put_str(P_SESSIONID, id);
	}
    } else if (!_strnicmp(hdr, X_SERVER_STATUS_HEADER,
	    strlen(X_SERVER_STATUS_HEADER))) {
	int rc;

	rc = http_parse_error_from_server(hdr);
	exit(rc);
    }

    return size * nitems;
}

/// Reads an HTTP body (in potentially many callback chunks) and
/// concatenates them into a single null-terminated malloc-ed char buffer,
/// or optionally throws the data away.
/// Based on getinmemory.c from the libcurl examples.
/// @param[in] bptr     points to the current body fragment
/// @param[in] size     the size of each 'item' in the body fragment
/// @param[in] nitems   the number of 'items' in the body fragment
/// @param[in] userp    points to a structure for holding the full body
/// @return size * nitems
size_t
http_read_body(void *bptr, size_t size, size_t nitems, void *userp)
{
    size_t chunksize;
    http_body_s *cp;

    chunksize = size * nitems;
    cp = (http_body_s *) userp;

    if (cp) {
	// Expand memory allocation to fit the new chunk plus terminating NUL.
	cp->hb_buffer = (CS)putil_realloc(cp->hb_buffer,
					  cp->hb_size + chunksize +
					  CHARSIZE);

	// Append the new chunk plus NUL.
	memcpy(&(cp->hb_buffer[cp->hb_size]), bptr, chunksize);
	cp->hb_size += chunksize;
	cp->hb_buffer[cp->hb_size] = '\0';
    } else {
	// Discard the response body - there are times when we need to.
    }

    return chunksize;
}

// Internal service routine.
static size_t
_http_action_headers(void *vptr, size_t size, size_t nitems, void *userp)
{
    UNUSED(vptr);
    UNUSED(userp);
    return size * nitems;
}

// Internal service routine.
static int
_http_action_connect(char **uptr, CS const *argv, int statfiles)
{
    CURL *curl;
    TCHAR rwdbuf[PATH_MAX];
    CS projname = NULL;
    CCS ofile;
    int rc = 0;

    // Special case - these flags are known to both client and server
    // (with the same semantics) so in order to avoid confusion for
    // users we pass client-side settings along to the server.
    if (prop_is_true(P_ABSOLUTE_PATHS)) {
	http_add_param(uptr, ACTION_ARGS_PARAM, "-a");
    }

    if (prop_is_true(P_MEMBERS_ONLY)) {
	http_add_param(uptr, ACTION_ARGS_PARAM, "-m");
    }

    curl = http_get_curl_handle(1);

    // We parse through all remaining cmdline args looking for
    // "standard" options such as -p|--project-name. The rest
    // get thrown onto the URL as ARGS parameters. In fact, and
    // simply because it's easier than picking them apart, even
    // flags parsed by getopt are set redundantly as parameters.
    if (argv && *argv) {
	int argc, c;

	// Re-create a valid argc value.
	for (argc = 0; argv[argc]; argc++);

	for (bsd_getopt_reset(), bsd_opterr = 0; ;) {
	    // *INDENT-OFF*
	    static CS short_opts = _T("+p:");
	    static struct option long_opts[] = {
		{_T("project-name"),	required_argument, NULL, 'p'},
		{0,			0,		   NULL,  0 },
	    };
	    // *INDENT-ON*

	    c = bsd_getopt(argc + 1, argv - 1, short_opts, long_opts, NULL);
	    if (c == -1) {
		break;
	    }

	    switch (c) {
		case 'p':
		    projname = bsd_optarg;
		    break;
		default:
		    break;
	    }
	}

	// Stick the argv onto the URL as the "ARGS" array.
	for (; *argv; argv++) {
	    http_add_param(uptr, ACTION_ARGS_PARAM, *argv);

	    if (statfiles) {
		ps_o ps;
		CCS psbuf;

		ps = ps_newFromPath(*argv);
		if (ps_stat(ps, 1)) {
		    rc = 1;
		} else {
		    psbuf = ps_tostring(ps);
		    http_add_header(curl, X_PATHSTATE_HEADER, psbuf);
		    putil_free(psbuf);
		}
		ps_destroy(ps);
	    }
	}
    } else {
	http_add_param(uptr, ACTION_ARGS_PARAM, _T(""));
    }

    // The server must be told how to format paths (/ or \).
    http_add_param(uptr, HTTP_CLIENT_PLATFORM_PARAM,
			prop_get_str(P_CLIENT_PLATFORM));

    // Add any other generic params the server may be interested in ...

    if (projname) {
	http_add_param(uptr, HTTP_PROJECT_NAME_PARAM, projname);
    } else {
	http_add_param(uptr, HTTP_PROJECT_NAME_PARAM,
		       prop_get_str(P_PROJECT_NAME));
    }

    http_add_param(uptr, HTTP_RWD_PARAM, util_get_rwd(rwdbuf, charlen(rwdbuf)));

    // This will set the output file pointer to either stdout or 
    // the path specified by the property.
    if ((ofile = prop_get_str(P_OUTPUT_FILE))) {
	Output_FP = util_open_output_file(ofile);
    } else {
	Output_FP = stdout;
    }

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, _http_action_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, NULL);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rc);

    if (http_connect(curl, *uptr, 1) && rc == 0) {
	rc = 2;
    }

    return rc;
}

/// Helps to apply a label to an existing PTX via the "label" action.
/// @param[in] label    the tag to apply to the PTX
/// @param[in] argv     a classic NULL-terminated arg vector
/// @return 0 on success
int
http_label(CCS label, CS const *argv)
{
    char *url;
    int rc;

    url = _http_make_url(ACTION_SERVLET_PREFIX, LABEL_SERVLET_NICKNAME);

    http_add_param(&url, HTTP_LABEL_PARAM, label);

    rc = _http_action_connect(&url, argv, 0);

    return rc;
}

/// Helps to apply a label to path states on the server side by calling
/// the "namestate" action on the client side.
/// @param[in] name             the tag to apply to the PS record
/// @param[in] pathstate        a stringified PS object
/// @param[in] argv             a classic NULL-terminated arg vector
/// @return 0 on success
int
http_namestate(CCS name, CCS pathstate, CS const *argv)
{
    char *url;
    int rc;

    url = _http_make_url(ACTION_SERVLET_PREFIX, NAMESTATE_SERVLET_NICKNAME);

    http_add_param(&url, HTTP_PS_NAME_PARAM, name);
    http_add_param(&url, HTTP_PS_CSV_PARAM, pathstate);

    rc = _http_action_connect(&url, argv, 0);

    return rc;
}

/// Executes an action by passing it to the server and printing the results.
/// @param[in] action   a string representing the action name
/// @param[in] argv     a classic NULL-terminated arg vector
/// @return 0 on success
int
http_action(CCS action, CS const *argv, int statfiles)
{
    char *url;
    int rc;

    url = _http_make_url(ACTION_SERVLET_PREFIX, action);

    rc = _http_action_connect(&url, argv, statfiles);

    return rc;
}

/// Used to keep check silently whether the server is alive.
/// @return 0 if server is listening
int
http_ping(void)
{
    CURL *curl;
    char *url;
    int rc = 0;

    curl = http_get_curl_handle(1);

    url = _http_make_url(ACTION_SERVLET_PREFIX, "ping");

    // Sending NULL to the callback will cause it to discard the response.
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_read_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

    if (http_connect(curl, url, 1) && rc == 0) {
	rc = 2;
    }

    return rc;
}

/// Used to keep HTTP sessions alive during a long-running transaction.
/// @param[in] secs     number of seconds triggering this heartbeat
/// @return 0 on success
int
http_heartbeat(int64_t secs)
{
    vb_printf(VB_ON, "Heartbeat! (%lld seconds)", secs);
    return http_ping();
}

/// Asks the web container to stop and start the AO context.
/// This requires some server-side config to create a user and
/// assign it the "manager" role. It's not clear how Tomcat-
/// centric this is; for Tomcat it requires a mod to tomcat-users.xml
/// and of course depends on the manager webapp to be enabled.
/// Seems likely other containers have something similar.
/// This is not an integral part of the system, just a dev convenience.
/// @return 0 on success
int
http_restart(void)
{
    CCS app;
    const char *svr;
    const char *mgrfmt = "http://%s/manager/%s?path=/%s";
    CURL *curl;
    char *url;
    int rc = 0;

    curl = http_get_curl_handle(1);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, stderr);
    curl_easy_setopt(curl, CURLOPT_USERPWD, "AO:AO");

    svr = prop_get_strA(P_SERVER);

    if (!(app = prop_get_strA(P_SERVER_CONTEXT))) {
	app = prop_get_app();
    }

    if (asprintf(&url, mgrfmt, svr, "stop", app) < 0) {
	putil_syserr(2, NULL);
    }

    if (http_connect(curl, url, 1)) {
	rc = 2;
    } else {
	putil_free(url);
	if (asprintf(&url, mgrfmt, svr, "start", app) < 0) {
	    putil_syserr(2, NULL);
	}
	if (http_connect(curl, url, 1)) {
	    rc = 2;
	}
    }

    putil_free(url);

    return rc;
}
