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

#ifndef HTTP_H
#define HTTP_H

/// @file
/// @brief Declarations for http.c

#include <sys/types.h>

#include "curl/curl.h"

/// @cond HTTP
#define HTTP_AGGRESSIVE_PARAM			"aggressive"
#define HTTP_BASE_DIR_PARAM			"base_dir"
#define HTTP_CLIENT_START_TIME_PARAM		"start_time"
#define HTTP_CLIENT_VERSION_PARAM		"clientver"
#define HTTP_GROUP_NAME_PARAM			"group"
#define HTTP_GZIPPED_PARAM			"gzipped"
#define HTTP_HOST_NAME_PARAM			"hostname"
#define HTTP_LABEL_PARAM			"label"
#define HTTP_LOG_LEVEL_PARAM			"loglevel"
#define HTTP_LOGFILE_PARAM			"logfile"
#define HTTP_LOGIN_NAME_PARAM			"logname"
#define HTTP_MACHINE_TYPE_PARAM			"machine"
#define HTTP_OS_RELEASE_PARAM			"osrelease"
#define HTTP_PROGRAM_NAME_PARAM			"prog"
#define HTTP_PROJECT_NAME_PARAM			"project_name"
#define HTTP_PS_CSV_PARAM			"pathstate"
#define HTTP_PS_NAME_PARAM			"name"
#define HTTP_PTX_STRATEGY_PARAM			"ptx_strategy"
#define HTTP_READ_ONLY_PARAM			"readonly"
#define HTTP_RWD_PARAM				"rwd"
#define HTTP_SHOP_MEMBERS_ONLY_PARAM		"shop_members_only"
#define HTTP_SYSTEM_NAME_PARAM			"sysname"
#define HTTP_SESSION_TIMEOUT_SECS_PARAM		"session_timeout_secs"
#define HTTP_UNCOMPRESSED_TRANSFERS_PARAM	"uncompressed_transfers"
#define HTTP_CLIENT_PLATFORM_PARAM		"client_platform"

#define ROADMAP_SERVLET_NICKNAME		"ROADMAP"
#define SESSION_SERVLET_NICKNAME		"SESSION"
#define START_SERVLET_NICKNAME			"START"
#define CHECK_SERVLET_NICKNAME			"CHECK"
#define AUDIT_SERVLET_NICKNAME			"AUDIT"
#define END_SERVLET_NICKNAME			"END"
#define DOWNLOAD_SERVLET_NICKNAME		"DOWNLOAD"
#define UPLOAD_SERVLET_NICKNAME			"UPLOAD"

/// @endcond HTTP

/// A parameter value which will be interpreted as true by the server.
#define HTTP_TRUE				"1"

/// A magic string which, when found in an HTTP body, indicates that the
/// current line is an error message to be printed to stderr, and which
/// should cause the action to give a return code indicating error.
#define HTTP_ERROR				"<<-ERROR->>: "

/// A magic string which, when found in an HTTP body, indicates that the
/// current line is a warning message to be printed to stderr but which
/// has no effect on the return code.
#define HTTP_WARNING				"<<-WARNING->>: "

/// A magic string which, when found in an HTTP body, indicates that the
/// current line is an informational message to be printed to stdout.
#define HTTP_NOTE				"<<-NOTE->>: "

/// A custom-defined header which reports a success/failure code
/// from server to client.
#define X_SERVER_STATUS_HEADER			"X-Server-Status"

/// A custom-defined header which reports a success/failure code
/// from client to server.
#define X_CLIENT_STATUS_HEADER			"X-Client-Status"

/// A custom-defined header which reports a count of recycled files
/// from client to server.
#define X_RECYCLED_COUNT_HEADER			"X-Recycled-Count"

/// A custom-defined header which carries a property value from the server.
#define X_SET_PROPERTY_HEADER			"X-Set-Property"

/// A custom-defined header which specifies a stringified PathState to a
/// POST, where a parameter cannot be used because it drains the body too.
#define X_PATHSTATE_HEADER			"X-PathState"

/// A custom-defined header which specified a boolean indicating whether
/// the body is compressed using gzip encoding.
#define X_GZIPPED_HEADER			"X-GZIPPED"

/// A custom-defined header which specified a boolean indicating whether
/// the body represents a logfile.
#define X_LOGFILE_HEADER			"X-LOGFILE"

/// A standard header name.
#define CONTENT_TYPE_HEADER			"Content-Type"

/// A standard MIME type for generic data.
#define APPLICATION_OCTET_STREAM		"application/octet-stream"

/// The default prediction for how long a standard server session lasts
/// before timeout. This number is enforced in the server's deployment
/// descriptor.
#define HTTP_SESSION_TIMEOUT_SECS_DEFAULT	(30 * 60)

/// Structure used to pass around an HTTP body through callbacks.
/// Required because callback functions allow only one data pointer.
typedef struct {
    CS hb_buffer;		///< the body of a received HTTP transaction
    size_t hb_size;		///< the size of the HTTP body
} http_body_s;

/// Structure used to hold useful "extra" data attached to a libcurl
/// easy handle. Anything which must be explicitly freed when the
/// handle is freed should have a pointer here.
typedef struct {
    char *ci_url;			 ///< URL for the next connection
    struct curl_slist *ci_extra_headers; ///< a list of added HTTP headers
    void *ci_malloced;			 ///< pointer to be freed at cleanup
    void *ci_mapaddr;			 ///< address to be unmapped at cleanup
    unsigned long ci_mapsize;		 ///< size of mapped region if set
    char ci_errbuf[CURL_ERROR_SIZE];	 ///< buffer for error messages
    char *ci_verbosity;			 ///< optional verbosity message
} conn_info_s;

extern void http_init(void);
extern conn_info_s * http_get_conn_info(CURL *);
extern void http_clear_conn_info(CURL *);
extern CURL *http_get_curl_handle(int);
extern CURL *http_async_get_free_curl_handle(void);
void http_async_add_handle(CURL *);
extern void http_async_transfer(int);
extern int http_connect(CURL *, char *, int);
extern char *http_make_url(const char *);
extern char *http_chomp(char *);
extern void http_add_header(CURL *, const char *, const char *);
extern void http_add_param(char **, CCS, CCS);
extern int http_parse_error_from_server(char *);
extern size_t http_find_errors(void *, size_t, size_t, void *);
extern size_t http_parse_startup_headers(void *, size_t, size_t, void *);
extern size_t http_read_body(void *, size_t, size_t, void *);
extern int http_action(CCS, CS const *, int);
extern int http_label(CCS, CS const *);
extern int http_namestate(CCS, CCS, CS const *);
extern int http_ping(void);
extern int http_heartbeat(int64_t);
extern int http_restart(void);
extern void http_fini(void);

#endif				/*HTTP_H */
