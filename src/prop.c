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
/// @brief Support for Properties objects.
/// These are analogous to Java Properties in the sense that the file
/// format is the same. Essentially, properties are a set of key-value
/// pairs describing local or personal configuration choices.
///
/// In the Java model the *FIRST* setting of a property wins.  In other
/// words once a property is set, subsequent settings are no-ops unless
/// the previous value is explicitly deleted in between. Functions for
/// overriding values are provided.
///
/// Properties can be marked as public, private, or internal depending
/// on whether the user is likely to have an interest in them. The
/// difference between private and internal is that private properties
/// could be promoted to public, whereas internal properties would
/// make no sense to expose.
///
/// Properties may optionally be exported, which causes them to be
/// made available to child processes (typically via the auditor).
///
/// All property names are case-insensitive but by convention we use
/// UPPERCASE for internal names and MixedCase for exposed names.

#include "AO.h"

#include "PREFS.h"
#include "PROP.h"

#if  defined(_WIN32)
#include <wchar.h>
#else	/*_WIN32*/
#if  !defined(BSD)
#include <alloca.h>
#endif	/*BSD*/
/// @cond static
extern char **environ;		// Win32 declares this in stdlib.h
/// @endcond static
#endif	/*_WIN32*/

#if  defined(_WIN32)
static WCHAR PropEnvPrefixW[PATH_MAX];
#endif	/*_WIN32*/
static char PropEnvPrefix[PATH_MAX];
static size_t PropEnvPrefixLen;

static void _prop_export(prop_e);

/// @cond static
#define DEFAULT_LIST_SEP	","

#define PROP_NAME_MAX		256
#define PROP_VAL_MAX		(PATH_MAX*2)
#define PROP_STR_MAX		(PROP_NAME_MAX + PROP_VAL_MAX + 2)

#define PROP_NULL		"<NULL>"
#define PROP_TRUE		"<TRUE>"
#define PROP_FALSE		"<FALSE>"
#define PROP_REQUIRED		"<REQUIRED>"

#define PROP_FLAG_INTERNAL	0x000
#define PROP_FLAG_PRIVATE	PROP_FLAG_INTERNAL
#define PROP_FLAG_PUBLIC	0x001
#define PROP_FLAG_EXPORT	0x002
/// @endcond static

// THESE MUST BE IN THE SAME ORDER AS THE ENUM LIST!
// *INDENT-OFF*
static struct {
    CS		 pr_name;
    CS		 pr_value;
    CS		 pr_desc;
    CS		 pr_dflt;
    int		 pr_flags;
    int		 pr_pad;
    prop_e	 pr_index;
} proptab[] = {
    {
	"APP",	// THIS PROPERTY IS SPECIAL
	NULL,
	"Base name of the application",
	"ao",
	PROP_FLAG_INTERNAL,
	0,
	P_APP,
    },
    {
	"Absolute.Paths",
	NULL,
	"Show member paths as absolute",
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_ABSOLUTE_PATHS,
    },
    {
	"Activation.Prog.RE",
	NULL,
	"Regular expression describing programs which trigger an audit",
	NULL,
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_ACTIVATION_PROG_RE,
    },
    {
	"Aggregated.Subcmd",
	NULL,
	"Boolean - disable build avoidance within aggregated subcommands",
	PROP_FALSE,
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_AGGREGATED_SUBCMD,
    },
    {
	"Aggregation.Line.Break.RE",
	NULL,
	"Break-aggregation RE based on cmd line",
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_LINE_BREAK_RE,
    },
    {
	"Aggregation.Line.Strong.RE",
	NULL,
	"Strong-aggregation RE based on cmd line",
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	NULL,
#else	/*_WIN32*/
	"^(?:(?:/usr|/usr/xpg4)?/bin/)?[a-z]*sh\\s+|libtool|^/\\S*/perl\\s+\\S+gcc",
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_LINE_STRONG_RE,
    },
    {
	"Aggregation.Line.Weak.RE",
	NULL,
	"Weak-aggregation RE based on cmd line",
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	"cmd\\.exe$",
#else	/*_WIN32*/
	NULL,
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_LINE_WEAK_RE,
    },
    {
	"Aggregation.Prog.Break.RE",
	NULL,
	"Break-aggregation RE based on prog name",
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	NULL,
#else	/*_WIN32*/
	"make$",
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_PROG_BREAK_RE,
    },
    {
	"Aggregation.Prog.Strong.RE",
	NULL,
	"Strong-aggregation RE based on prog name",
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	"(cl|link|msbuild|vcbuild|devenv)\\.(exe|com)$",
#else	/*_WIN32*/
	"(\\bcc|\\bCC|gcc|gcc-.*|[cg][+]{2}|[cg][+]{2}-.*|ccache)$",
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_PROG_STRONG_RE,
    },
    {
	"Aggregation.Prog.Weak.RE",
	NULL,
	"Weak-aggregation RE based on prog name",
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	"java\\.exe$",
#else	/*_WIN32*/
	"java$",
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_PROG_WEAK_RE,
    },
    {
	"Aggregation.Style",
	NULL,
	"Whether to aggregate all, some, or none",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_AGGREGATION_STYLE,
    },
    // This is an undocumented  development hack which may be removed in time.
    {
	"Aggressive.Server",
	NULL,
	"Temporary hack to request aggressive server-side optimization",
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_AGGRESSIVE_SERVER,
    },
    {
	"Audit.Ignore.Path.RE",
	NULL,
	"Regular expression matching pathnames to be completely ignored",
#if defined(_WIN32)
	"\\b(index\\.dat|BuildLog\\.htm|\\.rsp|\\.bak)$|\\.cmake\\.state",
#else	/*_WIN32*/
	"/tmp\\d+\\b|\\b(\\.bak|\\.BAK)|\\.cmake\\.state$",
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_AUDIT_IGNORE_PATH_RE,
    },
    {
	"Audit.Ignore.Prog.RE",
	NULL,
	"Regular expression matching programs to be completely ignored",
#if defined(_WIN32)
	"mspdbsrv",
#else	/*_WIN32*/
	NULL,
#endif	/*_WIN32*/
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_AUDIT_IGNORE_PROG_RE,
    },
    {
	"Audit.Only",
	NULL,
	"Audit file and command activity but do no uploads or downloads",
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_AUDIT_ONLY,
    },
    {
	"Base.Dir",
	NULL,
	"The root of this project tree",
	PROP_REQUIRED,
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_BASE_DIR,
    },
    {
	"Client.Host",
	NULL,
	"Host for intra-client audit delivery connections",
	"127.0.0.1",
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_CLIENT_HOST,
    },
    {
	"Client.Platform",
	NULL,
	"Type of client platform: Unix, Windows, Cygwin, ...",
	"u",
	PROP_FLAG_PRIVATE,
	0,
	P_CLIENT_PLATFORM,
    },
    {
	"Client.Port",
	NULL,
	"Port for intra-client audit delivery connections",
	"0xA0A0",		// A0A0 hex == 41120 decimal
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_CLIENT_PORT,
    },
    {
	"Client.Timeout.Secs",
	NULL,
	"How often to check health of audited process, in seconds",
	"30",
	PROP_FLAG_PRIVATE,
	0,
	P_CLIENT_TIMEOUT_SECS,
    },
    {
	"Dcode.All",
	NULL,
	"Derive the data-code for all involved files",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_DCODE_ALL,
    },
    {
	"Dcode.Cache.Secs",
	NULL,
	"Timestamp offset from start time for dcode cache",
	"-1",
	PROP_FLAG_PRIVATE,
	0,
	P_DCODE_CACHE_SECS,
    },
    {
	"DEPTH",
	NULL,
	"Special modifiable EV carrying the cmd depth",
	NULL,
	PROP_FLAG_INTERNAL | PROP_FLAG_EXPORT,
	10,
	P_DEPTH,
    },
    {
	"Doc.Pager",
	NULL,
	"Pipe help output through specified pager",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_DOC_PAGER,
    },
    // Documented as a boolean but there's an additional undocumented "2"
    // value which means to run without changing any server state at all.
    // In other words 2 allows us to query the server for the roadmap
    // and to grab some recycling from it, but it will have no record
    // of any of this. Intended for debugging use.
    {
	"Download.Only",
	NULL,
	"Allow downloads but no uploads",
	0,
	PROP_FLAG_PUBLIC,
	0,
	P_DOWNLOAD_ONLY,
    },
    {
	"Execute.Only",
	NULL,
	"Suppress auditing and just run the command",
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_EXECUTE_ONLY,
    },
    {
	"Git",
	NULL,
	"Boolean - pass audit data to git",
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_GIT,
    },
    {
	"Git.Dir",
	NULL,
	"The location of an optional Git repository",
	NULL,
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_GIT_DIR,
    },
    {
	"Identity.Hash",
	NULL,
	"Name of identity hash (CRC, SHA1, GIT)",
	"GIT",
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_IDENTITY_HASH,
    },
    {
	"Leave.Roadmap",
	NULL,
	"Boolean - don't unlink roadmap file when done",
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_LEAVE_ROADMAP,
    },
    {
	"Log.File",
	NULL,
	"Path for automatically generated logfile",
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_LOG_FILE,
    },
    {
	"Log.File.Temp",
	NULL,
	"Log to a temp file, uploaded and removed",
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_LOG_FILE_TEMP,
    },
    {
	"Make.Depends",
	NULL,
	"Dump makefile dependency info to .d files",
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_MAKE_DEPENDS,
    },
    {
	"Make.File",
	NULL,
	"Generate a Makefile in the named file",
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_MAKE_FILE,
    },
    {
	"Make.OneShell",
	NULL,
	"Ask make to use a single shell for each recipe",
	PROP_TRUE,
	PROP_FLAG_PRIVATE,
	0,
	P_MAKE_ONESHELL,
    },
    {
	"Members.Only",
	NULL,
	"Show and consider only project member files",
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_MEMBERS_ONLY,
    },
    {
	"MMap.Larger.Than",
	NULL,
	"Use memory mapping to read files larger than this size",
	"32768",
	PROP_FLAG_PRIVATE,
	0,
	P_MMAP_LARGER_THAN,
    },
    {
	"NO.MONITOR",
	NULL,
	"Boolean - dump truly raw audit data without aggregation",
	PROP_FALSE,
	PROP_FLAG_INTERNAL | PROP_FLAG_EXPORT,
	0,
	P_NO_MONITOR,
    },
    {
	"Original.Datestamp",
	NULL,
	"Boolean - set mod time of downloaded files back to uploaded time",
	"1",
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_ORIGINAL_DATESTAMP,
    },
    {
	"Output.File",
	NULL,
	"Dump output data to specified file",
	NULL,
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_OUTPUT_FILE,
    },
    {
	"PCCODE",
	NULL,
	"Special modifiable EV holding the parent cmd code",
	NULL,
	PROP_FLAG_INTERNAL | PROP_FLAG_EXPORT,
	64,	// Buffer big enough for SHA-256 hash.
	P_PCCODE,
    },
    {
	"PCMDID",
	NULL,
	"Special modifiable EV holding the parent pid",
	NULL,
	PROP_FLAG_INTERNAL | PROP_FLAG_EXPORT,
	10,
	P_PCMDID,
    },
    {
	"Perl.Cmd",
	NULL,
	"Name of or path to the preferred Perl binary",
	"perl",
	PROP_FLAG_PRIVATE,
	0,
	P_PERL_CMD,
    },
    {
	"Print.Elapsed",
	NULL,
	"Print the elapsed wall-clock time at exit",
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_PRINT_ELAPSED,
    },
    {
	"PROGNAME",
	NULL,
	"Name of the running program (best guess)",
	NULL,
	PROP_FLAG_INTERNAL,
	0,
	P_PROGNAME,
    },
    {
	"Project.Name",
	NULL,
	"Assign a name to this project",
	NULL,
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_PROJECT_NAME,
    },
    {
	"PTX.Strategy",
	NULL,
	"Selection criteria for downloadable PTXes",
	"30,-1,10,10",
	PROP_FLAG_PUBLIC,
	0,
	P_PTX_STRATEGY,
    },
    {
	"Reuse.Roadmap",
	NULL,
	"Boolean - use the pre-existing roadmap (debugging use only)",
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_REUSE_ROADMAP,
    },
    {
	"Roadmap.File",
	NULL,
	"Path to the file containing the CA/PS/PTX database",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_ROADMAPFILE,
    },
    {
	"Server",
	NULL,
	"Name of server in <host>:<port> format",
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_SERVER,
    },
    {
	"SERVER.CONTEXT",
	NULL,
	"The webapp 'context' string",
	"AO",
	PROP_FLAG_INTERNAL,
	0,
	P_SERVER_CONTEXT,
    },
    {
	"Server.Log.Level",
	NULL,
	"Server-side log4j level (OFF, ALL, DEBUG, ...)",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_SERVER_LOG_LEVEL,
    },
    {
	"SESSIONID",
	NULL,
	"The HTTP session id cookie value received from the server",
	NULL,
	PROP_FLAG_INTERNAL,
	0,
	P_SESSIONID,
    },
    {
	"Session.Timeout.Secs",
	NULL,
	"The HTTP session timeout used during a build, in seconds",
	"0",
	PROP_FLAG_PRIVATE,
	0,
	P_SESSION_TIMEOUT_SECS,
    },
    {
	"Shop.Ignore.Path.RE",
	NULL,
	"Regular expression matching pathnames to ignore when shopping",
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_SHOP_IGNORE_PATH_RE,
    },
    // This is here because of a Unix flaw: though some systems *track*
    // file times to the granularity of nanoseconds, the only
    // POSIX/SUS way of *setting* a timestamp is utimes() which can
    // handle only microseconds. Thus, comparing nanoseconds is unfair
    // because a command like "touch" cannot control them.
    {
	"Shop.Time.Precision",
	NULL,
	"Number of decimal digits to consider in timestamp comparisons",
	"6",
	PROP_FLAG_PRIVATE,
	0,
	P_SHOP_TIME_PRECISION,
    },
    {
	"Strict",
	NULL,
	"A shorthand for all strict options",
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT,
    },
    {
	"Strict.Audit",
	NULL,
	"Abort if any command cannot be audited",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT_AUDIT,
    },
    {
	"Strict.Download",
	NULL,
	"Abort if any audited objects cannot be downloaded",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT_DOWNLOAD,
    },
    {
	"Strict.Error",
	NULL,
	"Abort after any error message",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT_ERROR,
    },
    {
	"Strict.Upload",
	NULL,
	"Abort if any audited objects cannot be uploaded",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT_UPLOAD,
    },
    {
	"Synchronous.Transfers",
	NULL,
	"Upload files in the foreground, for debugging",
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_SYNCHRONOUS_TRANSFERS,
    },
    {
	"Uncompressed.Transfers",
	NULL,
	"Boolean - handle compression/decompression on server",
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_UNCOMPRESSED_TRANSFERS,
    },
    {
	"Upload.Only",
	NULL,
	"Disable downloads and build avoidance",
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_UPLOAD_ONLY,
    },
    {
	"Upload.Reads",
	NULL,
	"Boolean - upload files read as well as written",
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_UPLOAD_READS,
    },
    {
	"Verbosity",
	NULL,
	"Set verbosity flags",
	"STD",
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_VERBOSITY,
    },
    {
	"WFlag",
	NULL,
	"Extension flags directed to subsystems",
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_WFLAG,
    },
    { NULL, NULL, NULL, NULL, 0, 0, (prop_e)0}
};
// *INDENT-ON*

// Utility: switch one char for another
static void
_prop_replace_char(CS bp, char newchar, char oldchar)
{
    for (; *bp; bp++) {
	if (*bp == oldchar) {
	    *bp = newchar;
	}
    }
}

/// Constructor - creates a new (singleton) Properties object.
/// @param[in] app      the name of this application
void
prop_init(CCS app)
{
    assert(!proptab[P_APP].pr_value);
    proptab[P_APP].pr_value = util_strdown(putil_strdup(app));
    prop_assert(P_APP);

    // Initialize the prefix which is used for exported properties.
    snprintf(PropEnvPrefix, charlen(PropEnvPrefix),
	"_%s_", proptab[P_APP].pr_value);
    util_strup(PropEnvPrefix);
    PropEnvPrefixLen = strlen(PropEnvPrefix);
#if defined(_WIN32)
    (void)MultiByteToWideChar(CP_ACP, 0, PropEnvPrefix, -1,
	PropEnvPrefixW, charlen(PropEnvPrefixW));
#endif	/*_WIN32*/
}

// Retrieve a generic pointer by key.
static CCS
_prop_get_value(prop_e prop)
{
    if (!prop_get_app()) {
	putil_warn("uninitialized properties table");
    } else if (proptab[prop].pr_index != prop) {
	putil_die("properties table definition skew");
    }

    return proptab[prop].pr_value;
}

// Parses out a line from a *.properties file. Handles leading and
// trailing whitespace and delimiters as per the J2SE 1.4 Properties spec.
static int
_prop_process_line(CS line)
{
    char buf[4096];
    CS bp, ep;
    prop_e prop;

    snprintf(buf, charlen(buf), "%s", line);
    bp = buf;
    ep = endof(buf) - 1;

    if (ep == bp) {
	return 0;
    }

    while (ISSPACE(*ep)) {
	*ep-- = '\0';
    }

    while (ISSPACE(*bp)) {
	bp++;
    }

    if (*bp == '#' || *bp == '!' || *bp == '\0') {
	return 0;
    }

    for (ep = bp; *ep && !ISSPACE(*ep) && *ep != '=' && *ep != ':'; ep++) {
	if (*ep == '\\') {	// handle \-escapes in key
	    int i;

	    for (i = 0; ep[i]; i++) {
		ep[i] = ep[i + 1];
	    }
	}
    }

    if (*ep == '\0') {
	return 1;
    }

    if (ISSPACE(*ep)) {
	*ep++ = '\0';
	while (ISSPACE(*ep)) {
	    ep++;
	}
	if (*ep == '=' || *ep == ':') {
	    ep++;
	}
    } else {
	*ep++ = '\0';
    }

    while (ISSPACE(*ep)) {
	ep++;
    }

    prop = prop_from_name(bp);
    if (prop == P_BADPROP) {
	putil_warn("unrecognized property: %s", bp);
    } else {
	prop_put_str(prop, ep);
    }

    return 0;
}

/// Looks up a property by its string name.
/// @param[in] name     a string representing a property
/// @return the property
prop_e
prop_from_name(CCS name)
{
    int i;

    for (i = 0; proptab[i].pr_name; i++) {
	if (!stricmp(name, proptab[i].pr_name)) {
	    return (prop_e)i;
	}
    }

    return P_BADPROP;
}

/// Looks up a property by its string name and returns its value
/// @param[in] name     a string representing a property
/// @return the named property's value, or NULL
CCS
prop_value_from_name(CCS name)
{
    prop_e prop;

    prop = prop_from_name(name);
    if (prop != P_BADPROP) {
	return prop_get_str(prop);
    } else {
	return NULL;
    }
}

/// Looks up a property and returns its string name.
/// @param[in] prop     a property
/// @return the name of the property
CCS
prop_to_name(prop_e prop)
{
    return proptab[prop].pr_name;
}

/// Sets a boolean property to true.
/// @param[in] prop     a boolean-valued property
void
prop_set_true(prop_e prop)
{
    prop_put_ulong(prop, 1);
}

/// Sets a boolean property, replacing the pre-existing
/// value if present.
/// @param[in] prop     a boolean-valued property
void
prop_override_true(prop_e prop)
{
    prop_unset(prop, 0);
    prop_set_true(prop);
}

/// Returns true iff the boolean property specified is true.
/// Note that "true/false" and "yes/no" are accepted values.
/// Otherwise, non-zero integral values are considered true
/// and all others are considered false.
/// @param[in] prop     a boolean-valued property
/// @return 1 if the boolean property has a true value
int
prop_is_true(prop_e prop)
{
    return prop_get_ulong(prop) ? 1 : 0;
}

/// Returns true iff the property has been set to a value.
/// A zero-length string is considered the same as a null string,
/// i.e. having no value.
/// Does not look at default values, only explicit settings.
/// @param[in] prop     a property
/// @return 1 if the property has been given an explicit setting
int
prop_has_value(prop_e prop)
{
    CCS value;

    value = proptab[prop].pr_value;
    return value && *value;
}

/// Queries whether the property specified is public (aka documented).
/// @param[in] prop     a property
/// @return 1 if the specified property is public
int
prop_is_public(prop_e prop)
{
    return proptab[prop].pr_flags & PROP_FLAG_PUBLIC ? 1 : 0;
}

// Error handler for when a required property is found to be missing.
static void
_prop_report_missing(prop_e prop, int isfatal)
{
    if (isfatal) {
	putil_die("missing required property: %s", prop_to_name(prop));
    } else {
	putil_warn("missing required property: %s", prop_to_name(prop));
    }
}

// Internal support routine. Returns a property's value as a string.
static CCS
_prop_get_str(prop_e prop, int isfatal)
{
    CCS result, dflt;

    result = _prop_get_value(prop);
    dflt = proptab[prop].pr_dflt;

    if (result) {
	if (!strcmp(result, PROP_NULL)) {
	    result = NULL;
	}
    } else {
	result = dflt;
    }

    if (!result && dflt && streq(dflt, PROP_REQUIRED)) {
	_prop_report_missing(prop, isfatal);
    }

    return result;
}

/// Retrieves a string-valued property by key.
/// Feature: translates the value "<NULL>" to a null string. thus
/// allowing a null value to be set explicitly via e.g. "Foo = <NULL>".
/// @param[in] prop     a property
/// @return the value of the property as a string
const char *
prop_get_str(prop_e prop)
{
    CCS wval;

    if ((wval = _prop_get_str(prop, 1))) {
	return wval;
    } else {
	return NULL;
    }
}

/// Gets an unsigned numeric-valued property. Accepts values in decimal or hex.
/// @param[in] prop     a property
/// @return the value of the property as an unsigned long
unsigned long
prop_get_ulong(prop_e prop)
{
    CCS str;
    int base;

    if (!(str = prop_get_str(prop))) {
	return 0;
    }
    if (!stricmp(str, "true") || !stricmp(str, PROP_TRUE) ||
	!stricmp(str, "yes")) {
	return 1;
    }
    base = (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) ? 16 : 10;
    return strtoul(str, NULL, base);
}

/// Gets a signed numeric-valued property. Accepts values in decimal or hex.
/// @param[in] prop     a property
/// @return the value of the property as a signed long
long
prop_get_long(prop_e prop)
{
    CCS str;
    int base;

    if (!(str = prop_get_str(prop))) {
	return 0;
    }
    if (!stricmp(str, "true") || !stricmp(str, PROP_TRUE) ||
	!stricmp(str, "yes")) {
	return 1;
    }
    base = (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) ? 16 : 10;
    return strtol(str, NULL, base);
}

// Internal utility function to set a property, possibly exported.
static void
_prop_put_str(prop_e prop, CCS val)
{
    if (!_prop_get_value(prop)) {
	proptab[prop].pr_value = putil_strdup(val);
	if (proptab[prop].pr_flags & PROP_FLAG_EXPORT) {
	    _prop_export(prop);
	}
    }
}

/// Sets a string-valued property. Storage is allocated for the new string.
/// If the value is exported AND we may need to modify its value down the
/// road, it must be padded upon first export. This is because it's
/// crucial that the change not force growth in (and thus a potential
/// realloc of) the env block in an audited process. I.e. we may export
/// "1" as "         1" in order to reserve all the env space
/// possibly needed for a 32-bit value. Luckily, strtol and friends
/// ignore leading whitespace. For string-valured properties we will
/// have to remember to trim it off ourselves.
/// @param[in] prop     a property
/// @param[in] val      the new value for this property as a string
void
prop_put_str(prop_e prop, CCS val)
{
    if (proptab[prop].pr_pad > 0) {
	CS vbuf;
	size_t len;

	len = proptab[prop].pr_pad + CHARSIZE;
	vbuf = (CS)alloca(len);
	snprintf(vbuf, len, "%*s", proptab[prop].pr_pad, val);
	_prop_put_str(prop, vbuf);
    } else {
	_prop_put_str(prop, val);
    }
}

/// Sets a string numeric-valued property, replacing the pre-existing
/// value if present.
/// @param[in] prop     a property
/// @param[in] val      the new value for this property as a string
void
prop_override_str(prop_e prop, CCS val)
{
    prop_unset(prop, 0);
    prop_put_str(prop, val);
}

/// Sets a signed numeric-valued property. Integral-valued but
/// handled as a long for flexibility.
/// @param[in] prop     a property
/// @param[in] val      the new value for this property as a signed long
void
prop_put_long(prop_e prop, long val)
{
    char vbuf[64];

    snprintf(vbuf, charlen(vbuf), "%ld", val);
    prop_put_str(prop, vbuf);
}

/// Sets an unsigned numeric-valued property. Integral-valued but
/// handled as a long for flexibility.
/// @param[in] prop     a property
/// @param[in] val      the new value for this property as an unsigned long
void
prop_put_ulong(prop_e prop, unsigned long val)
{
    char vbuf[64];

    snprintf(vbuf, charlen(vbuf), "%lu", val);
    prop_put_str(prop, vbuf);
}

/// Sets an unsigned numeric-valued property, replacing the pre-existing
/// value if present.
/// @param[in] prop     a property
/// @param[in] val      the new value for this property as an unsigned long
void
prop_override_ulong(prop_e prop, unsigned long val)
{
    prop_unset(prop, 0);
    prop_put_ulong(prop, val);
}

/// Remove the value, if any, assigned to the specified property.
/// If it's an exported property, remove the associated environment
/// variable. WARNING: if called from within the auditor, the
/// unexport flag must be false, since removing EVs can trigger a
/// realloc with disastrous results. Maybe it would be better to
/// require a literal prop_unexport() where desired?
/// @param[in] prop     a property
/// @param[in] unexport boolean - true to remove corresponding EV
void
prop_unset(prop_e prop, int unexport)
{
    if (proptab[prop].pr_value) {
	putil_free(proptab[prop].pr_value);
	proptab[prop].pr_value = NULL;
	// If the user sets an unexported property explicitly via an EV
	// we do NOT want to remove it from the environment here.
	if (unexport && (proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
	    prop_unexport(prop, 0);
	}
    }
}

/// Returns the name of the current application.
/// Each Property 'object' has a special "app" property.
/// This value is prefixed to env vars representing properties.
/// @return the official application name
CCS
prop_get_app(void)
{
    return proptab[P_APP].pr_value ? proptab[P_APP].pr_value : proptab[P_APP].pr_dflt;
}

static int
_prop_matches_ev_name(CCS name)
{
    if (!strncmp(name, PropEnvPrefix, PropEnvPrefixLen)) {
	return PropEnvPrefixLen;
    } else if (!strncmp(name, PropEnvPrefix + 1, PropEnvPrefixLen - 1)) {
	return PropEnvPrefixLen - 1;
    } else {
	return 0;
    }
}

#if defined(_WIN32)
static int
_prop_matches_ev_nameW(WCHAR *name)
{
    if (!wcsncmp(name, PropEnvPrefixW, PropEnvPrefixLen)) {
	return PropEnvPrefixLen;
    } else if (!wcsncmp(name, PropEnvPrefixW + 1, PropEnvPrefixLen - 1)) {
	return PropEnvPrefixLen - 1;
    } else {
	return 0;
    }
}
#endif	/*_WIN32*/

// Internal service routine.
static void
_prop_load_from_ev(CS buf)
{
    CS bp;
    prop_e prop;

    if ((bp = strchr(buf, '=')) == NULL) {
	return;
    }

    *bp++ = '\0';
    _prop_replace_char(buf, '.', '_');
    prop = prop_from_name(buf);
    if (prop < 0) {
	putil_warn("unrecognized property: %s", buf);
    } else {
	// For consistency, if a property is inherited from the
	// environment but is not marked as exported, remove it from
	// the environment (but keep the setting). This allows a user
	// to set a property via the env without having different
	// semantics vs setting the same property in a file.
	// In other words only the top-level process finds it in the
	// env, while child processes use the normal inheritance path.
	if (!(proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
	    prop_unexport(prop, 0);
	}
	prop_put_str(prop, bp);
    }
}

/// Given a properties file, load it into the properties object.
/// @param[in] fname    the name of a properties file
/// @param[in] verbose  boolean verbosity flag
void
prop_load(CCS fname, CCS verbose)
{
    char buf[PROP_STR_MAX];

    if (!fname) {
	if (verbose) {
	    printf("%s[Environment]\n", verbose);
	}
    } else if (!access(fname, F_OK)) {
	if (verbose) {
	    printf("%s%s\n", verbose, fname);
	}
    } else {
	if (verbose) {
	    printf("%s%s (not present)\n", verbose, fname);
	}
	return;
    }

    if (fname) {
	FILE *fp;
	int ln;
	char pstr[PROP_STR_MAX], *tp, *bp;

	if ((fp = fopen(fname, "r"))) {
	    for (ln = 1; fgets(buf, charlen(buf), fp); ln++) {
		// In case a line-continuation (\) character is found.
		while ((bp = strchr(buf, '\n')) &&
			bp > buf && *(bp - 1) == '\\') {
		    *--bp = '\0';
		    if (!fgets(pstr, charlen(pstr), fp)) {
			break;
		    }
		    ln++;
		    for (tp = pstr; ISSPACE(*tp); tp++);
		    strcpy(bp, tp);
		}
		if (_prop_process_line(buf)) {
		    putil_warn("malformed line (%d) in %s: '%s'", ln, fname,
			       buf);
		}
	    }
	    fclose(fp);
	} else {
	    putil_syserr(0, fname);
	}
    } else {
	int offset;

#if defined(_WIN32)
	char *env, *ep;

	if (!(env = GetEnvironmentStrings())) {
	    putil_syserr(2, "GetEnvironmentStrings");
	}

	for (ep = env; *ep; ep++) {
	    if ((offset = _prop_matches_ev_name(ep))) {
		strcpy(buf, ep + offset);
		_prop_load_from_ev(buf);
	    }
	    ep = strchr(ep, '\0');
	}

	FreeEnvironmentStrings(env);
#else	/*_WIN32*/
	char **envp;

	for (envp = environ; *envp; envp++) {
	    if ((offset = _prop_matches_ev_name(*envp))) {
		strcpy(buf, *envp + offset);
		_prop_load_from_ev(buf);
	    }
	}
#endif	/*_WIN32*/
    }
}

/// Die with a message unless the property has been assigned a value.
/// @param[in] prop     a property
void
prop_assert(prop_e prop)
{
    if (!prop_get_str(prop)) {
	_prop_report_missing(prop, 1);
    }
}

// A property name maps to an EV by adding a standard prefix,
// converting periods to underscores, and uppercasing the result.
static CS
_prop_to_ev(prop_e prop, CS buf, size_t len)
{
    snprintf(buf, len, "%s%s", PropEnvPrefix, prop_to_name(prop));
    _prop_replace_char(buf, '_', '.');
    util_strup(buf);
    return buf;
}

// Export the specified property to the environment, uppercased,
// with the appname prefixed, and all dots replaced with underscores.
static void
_prop_export(prop_e prop)
{
    char namebuf[PROP_NAME_MAX];
    char *name;
    CS str;
    CCS val;

    if (!(val = prop_get_str(prop))) {
	_prop_report_missing(prop, 1);
    }
    _prop_to_ev(prop, namebuf, charlen(namebuf));
    name = namebuf;

    /* These properties are public so leave off the underscore */
    if (prop == P_BASE_DIR || prop == P_PROJECT_NAME) {
	name++;
    }

#if defined(_WIN32)
    str = (CS)alloca(PROP_STR_MAX);
    if (GetEnvironmentVariable(name, str, PROP_STR_MAX) && !strcmp(str, val)) {
	return;
    }
    if (!SetEnvironmentVariable(name, val)) {
	putil_syserr(0, name);
    }
    if (GetEnvironmentVariable(name, str, PROP_STR_MAX)) {
	assert(!strcmp(str, val));
    }
#else	/*_WIN32*/
    if ((str = putil_getenv(name)) && !strcmp(str, val)) {
	return;
    }
    str = (CS)putil_calloc(strlen(name) + strlen(val) + 2, CHARSIZE);
    strcpy(str, name);
    strcat(str, "=");
    strcat(str, val);
    putil_putenv(str);
    // Despite the confusing putenv man page we must NOT free this str.
    // Unfortunately Sun's bcheck will show it as a memory leak.
    // Sadly, so does valgrind 3.4.1 on Linux ...
    //putil_free(str);
#endif	/*_WIN32*/

    return;
}

/// Make sure the specified property will not be exported to children.
/// This has no effect on the current setting of the property.
/// WARNING: must not be called from the auditor.
/// @param[in] prop     the specified property
/// @param[in] forever  boolean - if set, newly set values no longer exported
void
prop_unexport(prop_e prop, int forever)
{
    char buf[PROP_NAME_MAX];

    _prop_to_ev(prop, buf, charlen(buf));

#if defined(_WIN32)
    SetEnvironmentVariable(buf, NULL);
#else	/*_WIN32*/
    putil_unsetenv(buf);
#endif	/*_WIN32*/

    if (forever) {
	proptab[prop].pr_flags &= ~PROP_FLAG_EXPORT;
    }

    return;
}

/// Returns the 'desc' (description) field of the specified property.
/// @param[in] prop     a property
/// @return the property's description string
CCS
prop_desc(prop_e prop)
{
    return proptab[prop].pr_desc;
}

/// Prints the known set of properties and their current values.
/// @param[in] all      boolean - if true, show undocumented properties too
/// @param[in] verbose  boolean - if true, show property descriptions
/// @param[in] exe      the name of the current program
void
prop_help(int all, int verbose, CCS exe)
{
    int prop;
    char name[PROP_NAME_MAX];
    CCS val;

    if (verbose) {
	printf("\nPROPERTIES [current values]:\n\n");
    }

    for (prop = 0; proptab[prop].pr_name; prop++) {
	if (!((proptab[prop].pr_flags & PROP_FLAG_PUBLIC) || all)) {
	    continue;
	}

	if (!(val = _prop_get_str((prop_e)prop, 0))) {
	    val = PROP_NULL;
	}

	if (verbose && proptab[prop].pr_desc) {
	    char pfx;

	    if (all && (proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
		pfx = '*';
	    } else {
		pfx = '#';
	    }

	    printf("%c %s:\n", pfx, proptab[prop].pr_desc);
	}

	snprintf(name, sizeof(name), "%s:", proptab[prop].pr_name);
	printf("%-28s %s\n", name, val);

	if (verbose && proptab[prop + 1].pr_name) {
	    fputc('\n', stdout);
	}
    }

    if (verbose && exe) {
	printf("\nLOADED FROM:\n");
	prefs_init(exe, PROP_EXT, "\t");
    }
}

/// Dumps all known properties to stderr. A zero argument prints only
/// user-visible properties, non-zero prints internal ones too.
/// This is primarily a convenience to be called from a debugger.
/// @param[in] all      boolean - if true, show all properties
void
prop_dump(int all)
{
    prop_help(all, 0, NULL);
}

// Internal service routine.
static CCS
_prop_modify_env_val(char *const *envp, CCS name, CCS nval)
{
#if defined(_WIN32)
    return SetEnvironmentVariable(name, nval) ? name : NULL;
#else				/*!_WIN32 */
    char *const *ep;
    CS oval = NULL, result = NULL;
    size_t nlen, ovlen, nvlen;

    nlen = strlen(name);

    if (!envp) {
	envp = environ;
    }

    for (ep = envp; *ep; ep++) {
	if (!strncmp(*ep, name, nlen) && *(*ep + nlen) == '=') {
	    oval = *ep + nlen + 1;
	    if (strcmp(oval, nval)) {
		ovlen = strlen(oval);
		nvlen = strlen(nval);
		if (nvlen > ovlen) {
		    putil_int("EV %s has no room for value %s", name, nval);
		    break;
		} else {
		    snprintf(oval, ovlen + 1, "%*s", (int)ovlen, nval);
		    result = putil_getenv(name);
		    break;
		}
	    } else {
		result = putil_getenv(name);
		break;
	    }
	}
    }

    if (oval) {
	if (result) {
	    ovlen = strlen(oval);
	    nvlen = strlen(result);
	    if (nvlen != ovlen) {
		putil_int("EV %s failed to update ('%d' != '%d'",
			  name, (int)nvlen, (int)ovlen);
	    }
	}
    } else {
	putil_int("EV %s not found in specified env", name);
    }

    return (CCS)result;
#endif	/*_WIN32*/
}

/// Modifies a string-valued property in place. Main reason to do this is if
/// the property is exported. Doing a putenv of the new value can cause
/// the environment block to be realloc-ed, i.e. moved, which can
/// confuse the host process badly.
/// So sometimes we need to overwrite the existing env value instead.
/// Our own exported numeric properties are zero-padded in the
/// environment block to ensure room for new values.
/// @param[in] prop     a property
/// @param[in] val      the new value
/// @param[in,out] envp optionally, a pointer to a custom env block
void
prop_mod_str(prop_e prop, CCS val, char *const *envp)
{
    int ovlen;
    char nbuf[PROP_NAME_MAX];

    assert(proptab[prop].pr_flags & PROP_FLAG_EXPORT);

    if (!proptab[prop].pr_value) {
	putil_int("property %s is not set", proptab[prop].pr_name);
    }

    if (proptab[prop].pr_value != val) {
	if (strlen(val) > strlen(proptab[prop].pr_value)) {
	    putil_int("property '%s=%s' has no room for value '%s'",
		      proptab[prop].pr_name, proptab[prop].pr_value, val);
	}

	ovlen = strlen(proptab[prop].pr_value);
	snprintf(proptab[prop].pr_value, ovlen + 1, "%*s", ovlen, val);
    }

    _prop_to_ev(prop, nbuf, charlen(nbuf));
    if (!_prop_modify_env_val(envp, nbuf, val)) {
	putil_warn("can't update env var %s to '%s'", nbuf, val);
    }
}

/// Modifies an unsigned numeric-valued property in place.
/// @param[in] prop     a property
/// @param[in] val      the new value
/// @param[in,out] envp optionally, a pointer to a custom env block
void
prop_mod_ulong(prop_e prop, unsigned long val, char *const *envp)
{
    int ovlen;
    char vbuf[64];

    assert(proptab[prop].pr_flags & PROP_FLAG_EXPORT);

    if (!proptab[prop].pr_value) {
	putil_int("property %s not set", proptab[prop].pr_name);
    }
    // Figure out how long the existing value is and make sure the
    // new value has the exact same number of characters.
    ovlen = strlen(proptab[prop].pr_value);
    snprintf(vbuf, charlen(vbuf), "%lu", val);

    // Any overflows should be detected here.
    prop_mod_str(prop, vbuf, envp);
}

/////////////////////////////////////////////////////////////////////////////
// This is almost too ugly to describe. First the background: both Unix
// (execve) and Windows (CreateProcess) allow a program to specify a
// "custom" environment for a child process. This isn't too common but it
// does happen. One example is Perl, which at startup reads its environment
// into the %ENV hash. Throughout the run of that Perl process, changes to
// %ENV are *not* propagated to the "real" environment of that process.
// Instead, whenever another program is exec-ed, %ENV is composed into an
// array of C strings which are passed to execve.
//
// Our problem with this design is that AO passes some of its own parameters
// (aka properties) in the environment. Unfortunately anything exported
// by the audit lib is "under the radar" of %ENV and thus not represented
// and not passed on to children as expected. This problem is not just
// in Perl; it may occur whenever a process assembles a custom environment
// for its child, though more often than not the custom env will include all
// current EVs.
//
// This function handles the problem by making a copy of the custom env
// and adding all exportable property values to it. This "doubly custom
// environment" is used for CreateProcess or execve and freed after
// (or if, in the case of exec) they return.
//
// The code is ugly because Unix and Windows have different environment
// models. Unix has an 'envp', an array of string pointers just like an
// argv. There's no API requirement that the env strings themselves be
// contiguous, only (of course) that the pointer array is. On Windows the
// env is a "naked" block of null-terminated strings, naked meaning there's
// no array of pointers. Since no array exists, the strings *must* be
// packed into a contiguous "environment block". A further Windows
// requirement is that they be sorted in a "case-insensitive, Unicode
// order, without regard to locale".
//
// We implement this with two functions; one to calculate the space
// required for the new block and another which takes that space as a
// parameter and fills it in. This is in order to avoid doing a malloc
// here, which is tricky to free in the context of an exec and is also
// bad form. It also seems to trigger a bug in the Sun Studio 9 compiler.
// So the safer plan is for the caller to ask how much space is needed,
// then alloca() that much on its own stack, then ask us to fill in the
// new block.
//
// To satisy requirements of all platforms we allocate a contiguous
// block containing both the envp array and the strings themselves,
// both in sorted order. The first word of our custom block is a pointer
// to the location of the strings. The result looks like (all contiguous):
//
//      <pointer to string block>
//      <array of pointers>
//      <string block>
//
// Therefore, assuming the pointer to the custom block is called "ptr", a
// Unix caller can use "ptr + 1" to get the environment it wants while a
// Windows caller can use "*ptr". And they can both "free(ptr)" when
// done (if ptr is on the heap, which it may not be). The array of
// pointers is not strictly necessary on Windows but (a) it's good
// to have a design that works for both Unix and Windows and (b) the
// pointers are useful for the sorting, which IS required.
//
// This whole thing is even further complicated by the fact that Windows
// maintains two distinct environment blocks, one using narrow (8-bit)
// strings and the other for Unicode strings. Normally an application
// interacts only with the style it was compiled for itself; however
// the auditor is a special case because no matter how it was compiled,
// it may find itself injected into a process of the other type.
// Therefore we need to maintain two separate implementations. Imagine
// the auditor is using narrow chars and interposes on CreateProcessW(),
// which takes an optional parameter for an env block which will be
// in Unicode format. In this case we'll need to manipulate Unicode
// strings from MBCS (ANSI) code, which takes some fancy stepping.
//
// Note that these functions must be independent of character width
// compilation mode. In other words they must not use the char API,
// because they are and should be hardwired to a particular char width.
/////////////////////////////////////////////////////////////////////////////

/// Calculates a buffer big enough for the new env
/// block plus the 'envp' (char **) array which Unix uses and
/// Windows doesn't (but which we need for sorting on Windows anyway)
/// plus terminating nulls.
size_t
prop_new_env_block_sizeA(char *const *cenv)
{
    size_t plen;
    int prop;
    int charwidth = sizeof(char);

    // Figure out how many bytes are in the provided env.
    // We might end up counting properties twice for size
    // purposes but that's ok, the block is short-lived.
#if defined(_WIN32)
    char *sp;

    for (plen = 0, sp = (char *)cenv; *sp; sp += strlen(sp) + 1) {
	plen += (strlen(sp) + 1) * charwidth;
	plen += sizeof(char *);
    }
    // The Windows block will be terminated by double null chars.
    plen += charwidth;
#else	/*_WIN32*/
    char **ep;

    for (plen = 0, ep = (char **)cenv; *ep; ep++) {
	plen += strlen(*ep) + charwidth;
	plen += sizeof(char *);
    }
#endif	/*_WIN32*/

    // For the NULL pointer which terminates envp.
    // Remember that we construct an envp even for Windows which
    // doesn't use one because we need it for sorting, which
    // Windows does require.
    plen += sizeof(char **);

    // Add the additional bytes which must be exported from properties.
    for (prop = 0; proptab[prop].pr_name; prop++) {
	char name[PROP_NAME_MAX];

	if (!proptab[prop].pr_value ||
	    !(proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
	    continue;
	}
	_prop_to_ev((prop_e)prop, name, charlen(name));
	// Account for ["name" '=' "value" '\0']
	plen += (strlen(name) +
		 strlen(proptab[prop].pr_value) + 2) * charwidth;
	plen += sizeof(char *);
    }

    // Don't forget the extra pointer at the front for Unix/Win consistency.
    plen += sizeof(char **);

    return plen;
}

#if defined(_WIN32)
// UNICODE version for Windows only.
size_t
prop_new_env_block_sizeW(WCHAR *const *cenv)
{
    size_t plen;
    int prop;
    int charwidth = sizeof(WCHAR);

    // Figure out how many bytes are in the provided env.
    // We might end up counting properties twice for size
    // purposes but that's ok, the block is short-lived.
    WCHAR *sp;
    for (plen = 0, sp = (WCHAR *)cenv; *sp; sp += wcslen(sp) + 1) {
	plen += (wcslen(sp) + 1) * charwidth;
	plen += sizeof(WCHAR *);
    }
    // The Windows block will be terminated by double null chars.
    plen += charwidth;

    // For the NULL pointer which terminates envp.
    // Remember that we construct an envp even for Windows which
    // doesn't use one because we need it for sorting, which
    // Windows does require.
    plen += sizeof(WCHAR **);

    // Add the additional bytes which must be exported from properties.
    for (prop = 0; proptab[prop].pr_name; prop++) {
	char name[PROP_NAME_MAX];

	if (!proptab[prop].pr_value ||
	    !(proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
	    continue;
	}
	_prop_to_ev((prop_e)prop, name, charlen(name));
	// Account for ["name" '=' "value" '\0']
	plen += (strlen(name) +
		 strlen(proptab[prop].pr_value) + 2) * charwidth;
	plen += sizeof(WCHAR *);
    }

    // Don't forget the extra pointer at the front for Unix/Win consistency.
    plen += sizeof(WCHAR **);

    return plen;
}
#endif	/*_WIN32*/

// Internal service routine, ANSI version.
static int
_prop_qsort_nenvA(const void *p1, const void *p2)
{
    const char **lp = (const char **)p1;
    const char **rp = (const char **)p2;

#if defined(_WIN32)
    return stricmp(*lp, *rp);
#else	/*_WIN32*/
    return strcmp(*lp, *rp);
#endif	/*_WIN32*/
}

#if defined(_WIN32)
// Internal service routine, UNICODE version for Windows only.
static int
_prop_qsort_nenvW(const void *p1, const void *p2)
{
    const WCHAR **lp = (const WCHAR **)p1;
    const WCHAR **rp = (const WCHAR **)p2;

    return _wcsicmp(*lp, *rp);
}
#endif	/*_WIN32*/

/// Fills in the supplied block of memory with our special
/// "cross-platform environment block". See voluminous related comments.
// One thing this code doesn't deal with is a custom 'envp' on
// Unix which _omits_ LD_PRELOAD*. Seems like the right thing
// to do would be to force it back in but the problem hasn't
// come up yet and may never.
char *const *
prop_custom_envA(char **pblock, char *const *cenv)
{
    int prop, count, i;
    char *sp;
    char **envp, **tenvp, **ep;
    int charwidth = sizeof(char);

    // Figure out how many non-properties strings are in the provided env.
#if defined(_WIN32)
    for (count = 0, sp = (char *)cenv; *sp; sp += strlen(sp) + 1) {
	if (!_prop_matches_ev_name(sp)) {
	    count++;
	}
    }
#else	/*_WIN32*/
    for (count = 0, ep = (char **)cenv; *ep; ep++) {
	if (!_prop_matches_ev_name(*ep)) {
	    count++;
	}
    }
#endif	/*_WIN32*/

    // Figure out how many additional strings are to be exported
    // from properties.
    for (prop = 0; proptab[prop].pr_name; prop++) {
	if (proptab[prop].pr_value &&
	    (proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
	    count++;
	}
    }

    tenvp = ep = (char **)alloca((count + 1) * sizeof(void *));

    // Enter all the pre-existing EVs in the temp envp.
#if defined(_WIN32)
    for (sp = (char *)cenv; *sp; sp += strlen(sp) + 1) {
	if (!_prop_matches_ev_name(sp)) {
	    *ep++ = sp;
	}
    }
#else	/*_WIN32*/
    {
	char *const *op;

	for (op = cenv; *op; op++) {
	    if (!_prop_matches_ev_name(*op)) {
		*ep++ = *op;
	    }
	}
    }
#endif	/*_WIN32*/

    // Place the exportable property settings in the temp envp too.
    for (prop = 0; proptab[prop].pr_name; prop++) {
	char name[PROP_NAME_MAX], *value;
	size_t len;

	value = proptab[prop].pr_value;
	if (!value || !(proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
	    continue;
	}

	_prop_to_ev((prop_e)prop, name, charlen(name));

	// Combine them into a narrow-char environment assignment.
	len = strlen(name) + charwidth + strlen(value);
	*ep = (char *)alloca((len + 1) * charwidth);
	strcpy(*ep, name);
	strcat(*ep, "=");
	strcat(*ep, value);
	ep++;
    }

    // And terminate the temp envp.
    *ep = NULL;

    // Sorting is required on Windows, just a frill on Unix.
    qsort(tenvp, count, sizeof(char *), _prop_qsort_nenvA);

    // Now that the temp array is sorted, copy it along with
    // its strings into the real, custom env block.
    envp = (char **)pblock + 1;
    for (i = 0, sp = (char *)(envp + count + 1); i < count; i++) {
	strcpy(sp, tenvp[i]);
	envp[i] = sp;
	sp += strlen(sp) + 1;
    }

    pblock[0] = (char *)(envp + count + 1);

    return pblock;
}

#if defined(_WIN32)
// UNICODE version for Windows only.
WCHAR *const *
prop_custom_envW(WCHAR **pblock, WCHAR *const *cenv)
{
    int prop, count, i;
    WCHAR *sp;
    WCHAR **envp, **tenvp, **ep;
    int charwidth = sizeof(WCHAR);
    LPWSTR name_w, value_w;
    int widelen;

    // Figure out how many non-properties strings are in the provided env.
    for (count = 0, sp = (WCHAR *)cenv; *sp; sp += wcslen(sp) + 1) {
	if (!_prop_matches_ev_nameW(sp)) {
	    count++;
	}
    }

    // Figure out how many additional strings are to be exported
    // from properties.
    for (prop = 0; proptab[prop].pr_name; prop++) {
	if (proptab[prop].pr_value &&
	    (proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
	    count++;
	}
    }

    tenvp = ep = (WCHAR **)_alloca((count + 1) * sizeof(void *));

    // Enter all the pre-existing EVs in the temp envp.
    for (sp = (WCHAR *)cenv; *sp; sp += wcslen(sp) + 1) {
	if (!_prop_matches_ev_nameW(sp)) {
	    *ep++ = sp;
	}
    }

    // Place the exportable property settings in the temp envp too.
    for (prop = 0; proptab[prop].pr_name; prop++) {
	char name[PROP_NAME_MAX], *value;
	size_t len;

	value = proptab[prop].pr_value;
	if (!value || !(proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
	    continue;
	}

	_prop_to_ev((prop_e)prop, name, charlen(name));

	// Convert the environment variable name to wide characters.
	widelen = MultiByteToWideChar(CP_ACP, 0, name, -1, NULL, 0);
	name_w = (LPWSTR) _alloca(widelen * sizeof(WCHAR));
	widelen = MultiByteToWideChar(CP_ACP, 0, name, -1, name_w, widelen);
	    
	// Convert the property value to wide characters.
	widelen = MultiByteToWideChar(CP_ACP, 0, value, -1, NULL, 0);
	value_w = (LPWSTR) _alloca(widelen * sizeof(WCHAR));
	widelen = MultiByteToWideChar(CP_ACP, 0, value, -1, value_w, widelen);

	// Combine them into a wide-char environment assignment.
	len = wcslen(name_w) + charwidth + wcslen(value_w);
	*ep = (WCHAR *)alloca((len + 1) * charwidth);
	wcscpy(*ep, name_w);
	wcscat(*ep, L"=");
	wcscat(*ep, value_w);
	ep++;
    }

    // And terminate the temp envp.
    *ep = NULL;

    // Sorting is required on Windows, just a frill on Unix.
    qsort(tenvp, count, sizeof(WCHAR *), _prop_qsort_nenvW);

    // Now that the temp array is sorted, copy it along with
    // its strings into the real, custom env block.
    envp = (WCHAR **)pblock + 1;
    for (i = 0, sp = (WCHAR *)(envp + count + 1); i < count; i++) {
	wcscpy(sp, tenvp[i]);
	envp[i] = sp;
	sp += wcslen(sp) + 1;
    }

    pblock[0] = (WCHAR *)(envp + count + 1);

    return pblock;
}
#endif	/*_WIN32*/

/// Attempts to remove from the environment any properties which were
/// placed there earlier in this program. This is of no practical
/// use since it happens just before exit anyway. The idea is to
/// help suppress warnings about memory leaks deriving from putenv()
/// of malloc-ed data, hopefully making it easier to find real
/// memory leaks.
void
prop_unexport_all(void)
{
#if !defined(_WIN32)
    char **envp, *ptr;

    for (envp = environ; *envp; envp++) {
	if (!strncmp(*envp, PropEnvPrefix, PropEnvPrefixLen)) {
	    ptr = *envp;
	    vb_printf(VB_TMP, "unsetenv(%s)\n", ptr);
	    putil_unsetenv(*envp);
	    putil_free(*envp);
	}
    }
#endif	/*_WIN32*/
}

/// Finalizer - releases all storage of the "singleton properties object".
void
prop_fini(void)
{
    int i;

    for (i = 0; proptab[i].pr_name; i++) {
	prop_unset(proptab[i].pr_index, 0);
    }
}
