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
/// properties may optionally be exported, which causes them to be
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
extern TCHAR **environ;		// Win32 declares this in stdlib.h
/// @endcond static
#endif	/*_WIN32*/

#if  defined(_WIN32)
static WCHAR PropEnvPrefixW[PATH_MAX];
#endif	/*_WIN32*/
static TCHAR PropEnvPrefix[PATH_MAX];
static size_t PropEnvPrefixLen;

static void _prop_export(prop_e);

/// @cond static
#define DEFAULT_LIST_SEP	_T(",")

#define PROP_NAME_MAX		256
#define PROP_VAL_MAX		(PATH_MAX*2)
#define PROP_STR_MAX		(PROP_NAME_MAX + PROP_VAL_MAX + 2)

#define PROP_NULL		_T("<NULL>")
#define PROP_TRUE		_T("<TRUE>")
#define PROP_FALSE		_T("<FALSE>")
#define PROP_REQUIRED		_T("<REQUIRED>")

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
	_T("APP"),	// THIS PROPERTY IS SPECIAL
	NULL,
	_T("Base name of the application"),
	_T("ao"),
	PROP_FLAG_INTERNAL,
	0,
	P_APP,
    },
    {
	_T("Absolute.Paths"),
	NULL,
	_T("Show member paths as absolute"),
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_ABSOLUTE_PATHS,
    },
    {
	_T("Activation.Prog.RE"),
	NULL,
	_T("Regular expression describing programs which trigger an audit"),
	NULL,
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_ACTIVATION_PROG_RE,
    },
    {
	_T("Aggregated.Subcmd"),
	NULL,
	_T("Boolean - disable build avoidance within aggregated subcommands"),
	PROP_FALSE,
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_AGGREGATED_SUBCMD,
    },
    {
	_T("Aggregation.Line.Break.RE"),
	NULL,
	_T("Break-aggregation RE based on cmd line"),
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_LINE_BREAK_RE,
    },
    {
	_T("Aggregation.Line.Strong.RE"),
	NULL,
	_T("Strong-aggregation RE based on cmd line"),
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	NULL,
#else	/*_WIN32*/
	_T("^(?:(?:/usr|/usr/xpg4)?/bin/)?[a-z]*sh\\s+|libtool"),
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_LINE_STRONG_RE,
    },
    {
	_T("Aggregation.Line.Weak.RE"),
	NULL,
	_T("Weak-aggregation RE based on cmd line"),
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	_T("cmd\\.exe$"),
#else	/*_WIN32*/
	NULL,
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_LINE_WEAK_RE,
    },
    {
	_T("Aggregation.Prog.Break.RE"),
	NULL,
	_T("Break-aggregation RE based on prog name"),
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	NULL,
#else	/*_WIN32*/
	_T("make$"),
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_PROG_BREAK_RE,
    },
    {
	_T("Aggregation.Prog.Strong.RE"),
	NULL,
	_T("Strong-aggregation RE based on prog name"),
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	_T("(cl|link|msbuild|vcbuild|devenv)\\.(exe|com)$"),
#else	/*_WIN32*/
	_T("(\\bcc|gcc|gcc-.*|g\\+\\+|ccache)$"),
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_PROG_STRONG_RE,
    },
    {
	_T("Aggregation.Prog.Weak.RE"),
	NULL,
	_T("Weak-aggregation RE based on prog name"),
#if defined(_WIN32)
	// Windows patterns are always case-insensitive.
	_T("java\\.exe$"),
#else	/*_WIN32*/
	_T("java$"),
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC,
	0,
	P_AGGREGATION_PROG_WEAK_RE,
    },
    {
	_T("Aggregation.Style"),
	NULL,
	_T("Whether to aggregate all, some, or none"),
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_AGGREGATION_STYLE,
    },
    // This is an undocumented  development hack which may be removed in time.
    {
	_T("Aggressive.Server"),
	NULL,
	_T("Temporary hack to request aggressive server-side optimization"),
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_AGGRESSIVE_SERVER,
    },
    {
	_T("Audit.Ignore.Path.RE"),
	NULL,
	_T("Regular expression matching pathnames to be completely ignored"),
#if defined(_WIN32)
	_T("\\b(index\\.dat|BuildLog\\.htm|\\.rsp|\\.bak)$"),
#else	/*_WIN32*/
	_T("/tmp\\d+\\b|\\b(\\.bak|\\.BAK)$"),
#endif	/*_WIN32*/
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_AUDIT_IGNORE_PATH_RE,
    },
    {
	_T("Audit.Ignore.Prog.RE"),
	NULL,
	_T("Regular expression matching programs to be completely ignored"),
#if defined(_WIN32)
	_T("mspdbsrv"),
#else	/*_WIN32*/
	NULL,
#endif	/*_WIN32*/
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_AUDIT_IGNORE_PROG_RE,
    },
    {
	_T("Audit.Only"),
	NULL,
	_T("Audit file and command activity but do no uploads or downloads"),
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_AUDIT_ONLY,
    },
    {
	_T("Base.Dir"),
	NULL,
	_T("The root of this project tree"),
	PROP_REQUIRED,
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_BASE_DIR,
    },
    {
	_T("Client.Host"),
	NULL,
	_T("Host for intra-client audit delivery connections"),
	_T("127.0.0.1"),
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_CLIENT_HOST,
    },
    {
	_T("Client.Platform"),
	NULL,
	_T("Type of client platform: Unix, Windows, Cygwin, ..."),
	_T("u"),
	PROP_FLAG_PRIVATE,
	0,
	P_CLIENT_PLATFORM,
    },
    {
	_T("Client.Port"),
	NULL,
	_T("Port for intra-client audit delivery connections"),
	_T("0xA0A0"),		// A0A0 hex == 41120 decimal
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_CLIENT_PORT,
    },
    {
	_T("Client.Timeout.Secs"),
	NULL,
	_T("How often to check health of audited process, in seconds"),
	_T("30"),
	PROP_FLAG_PRIVATE,
	0,
	P_CLIENT_TIMEOUT_SECS,
    },
    {
	_T("Dcode.All"),
	NULL,
	_T("Derive the data-code for all involved files"),
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_DCODE_ALL,
    },
    {
	_T("Dcode.Cache.Secs"),
	NULL,
	_T("Timestamp offset from start time for dcode cache"),
	_T("-1"),
	PROP_FLAG_PRIVATE,
	0,
	P_DCODE_CACHE_SECS,
    },
    {
	_T("DEPTH"),
	NULL,
	_T("Special modifiable EV carrying the cmd depth"),
	NULL,
	PROP_FLAG_INTERNAL | PROP_FLAG_EXPORT,
	10,
	P_DEPTH,
    },
    {
	_T("Doc.Pager"),
	NULL,
	_T("Pipe help output through specified pager"),
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_DOC_PAGER,
    },
    // Documented as a boolean but there's an additional undocumented "2"
    // value which means to run without changing server state at all.
    // In other words "2" means we can query the server for the roadmap
    // and to grab some recycling from it, but it will have no record
    // of any of this.
    {
	_T("Download.Only"),
	NULL,
	_T("Allow downloads but no uploads"),
	0,
	PROP_FLAG_PUBLIC,
	0,
	P_DOWNLOAD_ONLY,
    },
    {
	_T("Execute.Only"),
	NULL,
	_T("Suppress auditing and just run the command"),
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_EXECUTE_ONLY,
    },
    {
	_T("Identity.Hash.Algorithm"),
	NULL,
	_T("Name of identity hash (CRC, SHA, Adler, ...)"),
	_T("CRC"),
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_IDENTITY_HASH_ALGORITHM,
    },
    {
	_T("Leave.Roadmap"),
	NULL,
	_T("Boolean - don't unlink roadmap file when done"),
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_LEAVE_ROADMAP,
    },
    {
	_T("Log.File"),
	NULL,
	_T("Path for automatically generated logfile"),
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_LOG_FILE,
    },
    {
	_T("Log.File.Temp"),
	NULL,
	_T("Log to a temp file which is uploaded and removed"),
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_LOG_FILE_TEMP,
    },
    {
	_T("Make.File"),
	NULL,
	_T("Dump makefile dependency info to file"),
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_MAKE_FILE,
    },
    {
	_T("Make.File.Full"),
	NULL,
	_T("Generate a useable makefile in file"),
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_MAKE_FILE_FULL,
    },
    {
	_T("Make.OneShell"),
	NULL,
	_T("Ask make to use a single shell for each recipe"),
	PROP_TRUE,
	PROP_FLAG_PRIVATE,
	0,
	P_MAKE_ONESHELL,
    },
    {
	_T("Members.Only"),
	NULL,
	_T("Show and consider only project member files"),
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_MEMBERS_ONLY,
    },
    {
	_T("MMap.Larger.Than"),
	NULL,
	_T("Use memory mapping to read files larger than this size"),
	_T("32768"),
	PROP_FLAG_PRIVATE,
	0,
	P_MMAP_LARGER_THAN,
    },
    {
	_T("NO.MONITOR"),
	NULL,
	_T("Boolean - dump truly raw audit data without aggregation"),
	PROP_FALSE,
	PROP_FLAG_INTERNAL | PROP_FLAG_EXPORT,
	0,
	P_NO_MONITOR,
    },
    {
	_T("Original.Datestamp"),
	NULL,
	_T("Boolean - set mod time of downloaded files back to uploaded time"),
	_T("1"),
	PROP_FLAG_PRIVATE | PROP_FLAG_EXPORT,
	0,
	P_ORIGINAL_DATESTAMP,
    },
    {
	_T("Output.File"),
	NULL,
	_T("Dump output data to specified file"),
	NULL,
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_OUTPUT_FILE,
    },
    {
	_T("PCCODE"),
	NULL,
	_T("Special modifiable EV holding the parent cmd code"),
	NULL,
	PROP_FLAG_INTERNAL | PROP_FLAG_EXPORT,
	64,	// Buffer big enough for SHA-256 hash.
	P_PCCODE,
    },
    {
	_T("PCMDID"),
	NULL,
	_T("Special modifiable EV holding the parent pid"),
	NULL,
	PROP_FLAG_INTERNAL | PROP_FLAG_EXPORT,
	10,
	P_PCMDID,
    },
    {
	_T("Perl.Cmd"),
	NULL,
	_T("Name of or path to the preferred Perl binary"),
	_T("perl"),
	PROP_FLAG_PRIVATE,
	0,
	P_PERL_CMD,
    },
    {
	_T("Print.Elapsed"),
	NULL,
	_T("Print the elapsed wall-clock time at exit"),
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_PRINT_ELAPSED,
    },
    {
	_T("PROGNAME"),
	NULL,
	_T("Name of the running program (best guess)"),
	NULL,
	PROP_FLAG_INTERNAL,
	0,
	P_PROGNAME,
    },
    {
	_T("Project.Name"),
	NULL,
	_T("Assign a name to this project"),
	NULL,
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_PROJECT_NAME,
    },
    {
	_T("PTX.Strategy"),
	NULL,
	_T("Selection criteria for downloadable PTXes"),
	_T("30,-1,10,10"),
	PROP_FLAG_PUBLIC,
	0,
	P_PTX_STRATEGY,
    },
    {
	_T("Reuse.Roadmap"),
	NULL,
	_T("Boolean - use the pre-existing roadmap (debugging use only)"),
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_REUSE_ROADMAP,
    },
    {
	_T("Roadmap.File"),
	NULL,
	_T("Path to the file containing the CA/PS/PTX database"),
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_ROADMAPFILE,
    },
    {
	_T("Server"),
	NULL,
	_T("Name of server in <host>:<port> format"),
	NULL,
	PROP_FLAG_PUBLIC,
	0,
	P_SERVER,
    },
    {
	_T("SERVER.CONTEXT"),
	NULL,
	_T("The webapp 'context' string"),
	"AO",
	PROP_FLAG_INTERNAL,
	0,
	P_SERVER_CONTEXT,
    },
    {
	_T("Server.Log.Level"),
	NULL,
	_T("Server-side log4j level (OFF, ALL, DEBUG, ...)"),
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_SERVER_LOG_LEVEL,
    },
    {
	_T("SESSIONID"),
	NULL,
	_T("The HTTP session id cookie value received from the server"),
	NULL,
	PROP_FLAG_INTERNAL,
	0,
	P_SESSIONID,
    },
    {
	_T("Session.Timeout.Secs"),
	NULL,
	_T("The HTTP session timeout used during a build, in seconds"),
	_T("0"),
	PROP_FLAG_PRIVATE,
	0,
	P_SESSION_TIMEOUT_SECS,
    },
    {
	_T("Shop.Ignore.Path.RE"),
	NULL,
	_T("Regular expression matching pathnames to ignore when shopping"),
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
	_T("Shop.Time.Precision"),
	NULL,
	_T("Number of decimal digits to consider in timestamp comparisons"),
	_T("6"),
	PROP_FLAG_PRIVATE,
	0,
	P_SHOP_TIME_PRECISION,
    },
    {
	_T("Strict"),
	NULL,
	_T("A shorthand for all strict options"),
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT,
    },
    {
	_T("Strict.Audit"),
	NULL,
	_T("Abort if any command cannot be audited"),
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT_AUDIT,
    },
    {
	_T("Strict.Download"),
	NULL,
	_T("Abort if any audited objects cannot be downloaded"),
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT_DOWNLOAD,
    },
    {
	_T("Strict.Error"),
	NULL,
	_T("Abort after any error message"),
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT_ERROR,
    },
    {
	_T("Strict.Upload"),
	NULL,
	_T("Abort if any audited objects cannot be uploaded"),
	NULL,
	PROP_FLAG_PRIVATE,
	0,
	P_STRICT_UPLOAD,
    },
    {
	_T("Synchronous.Transfers"),
	NULL,
	_T("Upload files in the foreground, for debugging"),
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_SYNCHRONOUS_TRANSFERS,
    },
    {
	_T("Uncompressed.Transfers"),
	NULL,
	_T("Boolean - handle compression/decompression on server"),
	PROP_FALSE,
	PROP_FLAG_PRIVATE,
	0,
	P_UNCOMPRESSED_TRANSFERS,
    },
    {
	_T("Upload.Only"),
	NULL,
	_T("Disable downloads and build avoidance"),
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_UPLOAD_ONLY,
    },
    {
	_T("Upload.Reads"),
	NULL,
	_T("Boolean - upload files read as well as written"),
	PROP_FALSE,
	PROP_FLAG_PUBLIC,
	0,
	P_UPLOAD_READS,
    },
    {
	_T("Verbosity"),
	NULL,
	_T("Set verbosity flags"),
	_T("STD"),
	PROP_FLAG_PUBLIC | PROP_FLAG_EXPORT,
	0,
	P_VERBOSITY,
    },
    {
	_T("WFlag"),
	NULL,
	_T("Extension flags directed to subsystems"),
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
_prop_replace_char(CS bp, TCHAR newchar, TCHAR oldchar)
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
    _sntprintf(PropEnvPrefix, charlen(PropEnvPrefix),
	_T("_%s_"), proptab[P_APP].pr_value);
    util_strup(PropEnvPrefix);
    PropEnvPrefixLen = _tcslen(PropEnvPrefix);
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
	putil_warn(_T("uninitialized properties table"));
    } else if (proptab[prop].pr_index != prop) {
	putil_die(_T("properties table definition skew"));
    }

    return proptab[prop].pr_value;
}

// Parses out a line from a *.properties file. Handles leading and
// trailing whitespace and delimiters as per the J2SE 1.4 Properties spec.
static int
_prop_process_line(CS line)
{
    TCHAR buf[4096];
    CS bp, ep;
    prop_e prop;

    _sntprintf(buf, charlen(buf), _T("%s"), line);
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
	putil_warn(_T("unrecognized property: %s"), bp);
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
	if (!_tcsicmp(name, proptab[i].pr_name)) {
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
    if (prop && prop != P_BADPROP) {
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
	putil_die(_T("missing required property: %s"), prop_to_name(prop));
    } else {
	putil_warn(_T("missing required property: %s"), prop_to_name(prop));
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
	if (!_tcscmp(result, PROP_NULL)) {
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
prop_get_strA(prop_e prop)
{
    CCS wval;

    if ((wval = _prop_get_str(prop, 1))) {
#if defined(_UNICODE)
	int wlen;
	char *aval;

	// Convert the Unicode value to its ANSI equivalent on the *heap*.
	// HACK: this is a leak!
	wlen = (wcslen(wval) + 1) * sizeof(WCHAR);
	aval = (char *)putil_malloc(wlen);
	if (wcstombs(aval, wval, wlen) < 0) {
	    putil_syserr(2, _T("wcstombs()"));
	}
	return aval;
#else	/*_UNICODE*/
	return wval;
#endif	/*_UNICODE*/
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
    if (!_tcsicmp(str, _T("true")) || !_tcsicmp(str, PROP_TRUE) ||
	!_tcsicmp(str, _T("yes"))) {
	return 1;
    }
    base = (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) ? 16 : 10;
    return _tcstoul(str, NULL, base);
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
    if (!_tcsicmp(str, _T("true")) || !_tcsicmp(str, PROP_TRUE) ||
	!_tcsicmp(str, _T("yes"))) {
	return 1;
    }
    base = (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) ? 16 : 10;
    return _tcstol(str, NULL, base);
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
	_sntprintf(vbuf, len, _T("%*s"), proptab[prop].pr_pad, val);
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
    TCHAR vbuf[64];

    _sntprintf(vbuf, charlen(vbuf), _T("%ld"), val);
    prop_put_str(prop, vbuf);
}

/// Sets an unsigned numeric-valued property. Integral-valued but
/// handled as a long for flexibility.
/// @param[in] prop     a property
/// @param[in] val      the new value for this property as an unsigned long
void
prop_put_ulong(prop_e prop, unsigned long val)
{
    TCHAR vbuf[64];

    _sntprintf(vbuf, charlen(vbuf), _T("%lu"), val);
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
    if (!_tcsncmp(name, PropEnvPrefix, PropEnvPrefixLen)) {
	return PropEnvPrefixLen;
    } else if (!_tcsncmp(name, PropEnvPrefix + 1, PropEnvPrefixLen - 1)) {
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

    if ((bp = _tcschr(buf, '=')) == NULL) {
	return;
    }

    *bp++ = '\0';
    _prop_replace_char(buf, '.', '_');
    prop = prop_from_name(buf);
    if (prop < 0) {
	putil_warn(_T("unrecognized property: %s"), buf);
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
    TCHAR buf[PROP_STR_MAX];

    if (!fname) {
	if (verbose) {
	    _tprintf(_T("%s[Environment]\n"), verbose);
	}
    } else if (!_taccess(fname, F_OK)) {
	if (verbose) {
	    _tprintf(_T("%s%s\n"), verbose, fname);
	}
    } else {
	if (verbose) {
	    _tprintf(_T("%s%s (not present)\n"), verbose, fname);
	}
	return;
    }

    if (fname) {
	FILE *fp;
	int ln;
	TCHAR pstr[PROP_STR_MAX], *tp, *bp;

	if ((fp = _tfopen(fname, _T("r")))) {
	    for (ln = 1; _fgetts(buf, charlen(buf), fp); ln++) {
		// In case a line-continuation (\) character is found.
		while ((bp = _tcschr(buf, '\n')) &&
			bp > buf && *(bp - 1) == '\\') {
		    *--bp = '\0';
		    if (!_fgetts(pstr, charlen(pstr), fp)) {
			break;
		    }
		    ln++;
		    for (tp = pstr; ISSPACE(*tp); tp++);
		    _tcscpy(bp, tp);
		}
		if (_prop_process_line(buf)) {
		    putil_warn(_T("malformed line (%d) in %s: '%s'"), ln, fname,
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
		_tcscpy(buf, ep + offset);
		_prop_load_from_ev(buf);
	    }
	    ep = _tcschr(ep, '\0');
	}

	FreeEnvironmentStrings(env);
#else	/*_WIN32*/
	TCHAR **envp;

	for (envp = _tenviron; *envp; envp++) {
	    if ((offset = _prop_matches_ev_name(*envp))) {
		_tcscpy(buf, *envp + offset);
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
    _sntprintf(buf, len, _T("%s%s"), PropEnvPrefix, prop_to_name(prop));
    _prop_replace_char(buf, '_', '.');
    util_strup(buf);
    return buf;
}

// Export the specified property to the environment, uppercased,
// with the appname prefixed, and all dots replaced with underscores.
static void
_prop_export(prop_e prop)
{
    TCHAR name[PROP_NAME_MAX];
    CS str;
    CCS val;

    if (!(val = prop_get_str(prop))) {
	_prop_report_missing(prop, 1);
    }
    _prop_to_ev(prop, name, charlen(name));

#if defined(_WIN32)
    str = (CS)alloca(PROP_STR_MAX);
    if (GetEnvironmentVariable(name, str, PROP_STR_MAX) && !_tcscmp(str, val)) {
	return;
    }
    if (!SetEnvironmentVariable(name, val)) {
	putil_syserr(0, name);
    }
    if (GetEnvironmentVariable(name, str, PROP_STR_MAX)) {
	assert(!_tcscmp(str, val));
    }
#else	/*_WIN32*/
    if ((str = putil_getenv(name)) && !_tcscmp(str, val)) {
	return;
    }
    str = (CS)putil_calloc(_tcslen(name) + _tcslen(val) + 2, CHARSIZE);
    _tcscpy(str, name);
    _tcscat(str, _T("="));
    _tcscat(str, val);
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
    TCHAR buf[PROP_NAME_MAX];

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
    TCHAR name[PROP_NAME_MAX];
    CCS val;

    if (verbose) {
	_tprintf(_T("\nPROPERTIES [current values]:\n\n"));
    }

    for (prop = 0; proptab[prop].pr_name; prop++) {
	if (!((proptab[prop].pr_flags & PROP_FLAG_PUBLIC) || all)) {
	    continue;
	}

	if (!(val = _prop_get_str((prop_e)prop, 0))) {
	    val = PROP_NULL;
	}

	if (verbose && proptab[prop].pr_desc) {
	    TCHAR pfx;

	    if (all && (proptab[prop].pr_flags & PROP_FLAG_EXPORT)) {
		pfx = '*';
	    } else {
		pfx = '#';
	    }

	    _tprintf(_T("%c %s:\n"), pfx, proptab[prop].pr_desc);
	}

	_sntprintf(name, sizeof(name), _T("%s:"), proptab[prop].pr_name);
	_tprintf(_T("%-28s %s\n"), name, val);

	if (verbose && proptab[prop + 1].pr_name) {
	    fputc('\n', stdout);
	}
    }

    if (verbose && exe) {
	_tprintf(_T("\nLOADED FROM:\n"));
	prefs_init(exe, PROP_EXT, _T("\t"));
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
		    putil_int(_T("EV %s has no room for value %s"), name, nval);
		    break;
		} else {
		    _sntprintf(oval, ovlen + 1, _T("%*s"), (int)ovlen, nval);
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
		putil_int(_T("EV %s failed to update ('%d' != '%d'"),
			  name, (int)nvlen, (int)ovlen);
	    }
	}
    } else {
	putil_int(_T("EV %s not found in specified env"), name);
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
    TCHAR nbuf[PROP_NAME_MAX];

    assert(proptab[prop].pr_flags & PROP_FLAG_EXPORT);

    if (!proptab[prop].pr_value) {
	putil_int(_T("property %s is not set"), proptab[prop].pr_name);
    }

    if (proptab[prop].pr_value != val) {
	if (_tcslen(val) > _tcslen(proptab[prop].pr_value)) {
	    putil_int(_T("property '%s=%s' has no room for value '%s'"),
		      proptab[prop].pr_name, proptab[prop].pr_value, val);
	}

	ovlen = _tcslen(proptab[prop].pr_value);
	_sntprintf(proptab[prop].pr_value, ovlen + 1, _T("%*s"), ovlen, val);
    }

    _prop_to_ev(prop, nbuf, charlen(nbuf));
    if (!_prop_modify_env_val(envp, nbuf, val)) {
	putil_warn(_T("can't update env var %s to '%s'"), nbuf, val);
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
    TCHAR vbuf[64];

    assert(proptab[prop].pr_flags & PROP_FLAG_EXPORT);

    if (!proptab[prop].pr_value) {
	putil_int(_T("property %s not set"), proptab[prop].pr_name);
    }
    // Figure out how long the existing value is and make sure the
    // new value has the exact same number of characters.
    ovlen = _tcslen(proptab[prop].pr_value);
    _sntprintf(vbuf, charlen(vbuf), _T("%lu"), val);

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
// compilation mode. In other words they must not use the TCHAR API,
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
    return _stricmp(*lp, *rp);
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
