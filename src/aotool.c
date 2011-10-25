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
/// @brief Contains the 'main' routine of the client.
/// Handles parsing of command-line options, certain
/// client-side-only actions, and general initialization
/// of the client program.

#include "AO.h"

#include "CA.h"
#include "CODE.h"
#include "DOWN.h"
#include "GIT.h"
#include "HTTP.h"
#include "MAKE.h"
#include "MON.h"
#include "PA.h"
#include "PN.h"
#include "PS.h"
#include "PREFS.h"
#include "PROP.h"
#include "SHOP.h"
#include "UW.h"

#include "cdb.h"
#include "curl/curl.h"
#include "pcre.h"
#include "zlib.h"
#include "openssl/opensslv.h"

#include "About/about.c"	// NOTE: including a .c file!

#include "bsd_getopt.h"

/// @cond static
#define ROADMAP_DEFAULT_NAME		"roadmap.cdb"
/// @endcond static

/// Macro for making unique long flags with getopt_long(). Some long
/// flags do not have a corresponding single-char name. We create an
/// artificial name for these by combining the first letter of the
/// long flag with an arbitrary second char. This provides a
/// 'namespace' allowing up to 256 different long flags for each
/// leading letter.
/// Note that the option.val member which holds this is really an int.
#define LF(first, secondary)	(((first)<<8) + (secondary))

// The time when the program started.
static long ElapsedMinimum;
static time_t StartTime;

// Hack for local shutupitude.
static int QuietMode;

// Internal service routine.
static void
_print_elapsed_time(void)
{
    util_print_elapsed(StartTime, ElapsedMinimum, "ELAPSED");
}

// Internal service routine. Print a usage message line for a property.
static void
_usg_line(FILE *fp, CCS fmt, CCS flag, prop_e prop, CCS desc)
{
    if (!prop_is_public(prop)) {
	return;
    }

    if (desc) {
	_ftprintf(fp, fmt, flag, desc);
    } else {
	TCHAR buf[1024];

	_sntprintf(buf, charlen(buf), _T("%s [=%s]"),
	    prop_desc(prop), prop_to_name(prop));
	_ftprintf(fp, fmt, flag, buf);
    }
}

// Internal service routine.
static void
_usage(int rc)
{
    FILE *fp;
    CCS fmt = _T("   %-15s %s\n");
    CCS prog;

    prog = prop_get_str(P_PROGNAME);

    fp = rc ? stderr : stdout;

    _ftprintf(fp, _T("USAGE: %s [<options>]"), prog);
    _ftprintf(fp, _T(" { make | run <prog> | <action> } [args...]\n"));
    _ftprintf(fp, _T("FLAGS:\n"));

    _ftprintf(fp, fmt, _T("-h"), _T("Print this usage summary"));
    _ftprintf(fp, fmt, _T("-H"), _T("Print current properties"));
    _usg_line(fp, fmt, _T("-a"), P_ABSOLUTE_PATHS, NULL);
    _usg_line(fp, fmt, _T("-d"), P_DOWNLOAD_ONLY, NULL);
    _usg_line(fp, fmt, _T("-F file"), P_MAKE_FILE, NULL);
#if !defined(_WIN32)
    _usg_line(fp, fmt, _T("-l file"), P_LOG_FILE, NULL);
    _usg_line(fp, fmt, _T("-L"), P_LOG_FILE_TEMP, NULL);
#endif	/*_WIN32*/
    _usg_line(fp, fmt, _T("-MD"), P_MAKE_DEPENDS, NULL);
    _usg_line(fp, fmt, _T("-m"), P_MEMBERS_ONLY, NULL);
    _usg_line(fp, fmt, _T("-o file"), P_OUTPUT_FILE, NULL);
    _usg_line(fp, fmt, _T("-p name"), P_PROJECT_NAME, NULL);
    _ftprintf(fp, fmt, _T("-q"), _T("Quiet mode: suppress verbosity"));
    _usg_line(fp, fmt, _T("-s host:port"), P_SERVER, NULL);
    _usg_line(fp, fmt, _T("-T"), P_PRINT_ELAPSED, NULL);
    _usg_line(fp, fmt, _T("-u"), P_UPLOAD_ONLY, NULL);
    _ftprintf(fp, fmt, _T("-vXX,YY,ZZ"),
	_T("Set verbosity flags (use -v? to see choices)"));
    _ftprintf(fp, fmt, _T("--version"), _T("Print version and exit"));
    _ftprintf(fp, fmt, _T("-w"), _T("Explain why commands cannot be recycled"));
    _ftprintf(fp, fmt, _T("-Wm,flag,value"),
	_T("Pass -flag=value to makefile generator"));
    _ftprintf(fp, fmt, _T("-x"), _T("Print each command line as executed"));
    _usg_line(fp, fmt, _T("-X"), P_EXECUTE_ONLY, NULL);

    _ftprintf(fp, _T("EXAMPLES:\n"));
    _ftprintf(fp, _T("   %s help\n"), prog);
    _ftprintf(fp, _T("   %s ping\n"), prog);
    _ftprintf(fp, _T("   %s -o@ -F Makefile.new make -s clean all\n"), prog);
    _ftprintf(fp, _T("   %s make clean all\n"), prog);
    _ftprintf(fp, _T("   %s lsbuilds -s\n"), prog);

    putil_exit(rc);
}

// Internal service routine. TODO Not yet re-enabled after persistence
// changes - this is a stub left around pending server/schema work.
static int
_name_pathstate(CCS name, CCS path, CS const *argv)
{
    struct __stat64 stbuf;
    ps_o ps;
    TCHAR psbuf[PATH_MAX + 256];
    TCHAR abspath[PATH_MAX];
    int rc = 0;

    // If we can't resolve the file vs the cwd, try vs the rwd.
    if (_tstat64(path, &stbuf)) {
	if (putil_is_absolute(path)) {
	    putil_syserr(0, path);
	    return 2;
	} else {
	    _sntprintf(abspath, charlen(abspath), _T("%s/%s"),
		       prop_get_str(P_BASE_DIR), path);
	    if (_tstat64(abspath, &stbuf)) {
		putil_syserr(0, path);	// NOT abspath
		return 2;
	    } else {
		ps = ps_newFromPath(abspath);
	    }
	}
    } else {
	ps = ps_newFromPath(path);
    }

    if (!S_ISREG(stbuf.st_mode)) {
	putil_error("not a regular file: %s", ps_get_abs(ps));
	ps_destroy(ps);
	return 2;
    }

    if (ps_stat(ps, 1)) {
	putil_syserr(0, ps_get_abs(ps));
	return 2;
    }

    ps_toCSVString(ps, psbuf, charlen(psbuf));

    rc = http_namestate(name, psbuf, argv);

    ps_destroy(ps);

    return rc;
}

/// Prints the version of this application to stdout.
/// @param[in] full      if true, print versions of all 3rd-party libs too
static void
_print_version(int full)
{
    _tprintf(_T("%s=%s\n"),
	     _T(APPLICATION_NAME), _T(APPLICATION_VERSION));

    if (full) {
	_fputts(putil_builton(), stdout);
#if defined(_WIN32) && defined(_DEBUG)
	_fputts(" (DEBUG)", stdout);
#endif	/*_DEBUG*/
	_fputts("\n", stdout);
	printf("libcurl=%s\n", curl_version());
	printf("pcre=%s\n", pcre_version());
	printf("zlib=%s\n", (const char *)zlibVersion());
	printf("libcrypto=%p\n", OPENSSL_VERSION_NUMBER);
	printf("tinycdb=%.3f\n", TINYCDB_VERSION);
	printf("kazlib=1.20\n"); // Abandoned, will never change
	printf("trio=1.14\n");	// TODO - hack
    }
}

/// Internal service routine. Handles sub-commands.
static int
do_action(CCS action, int argc, CS const *argv)
{
    int rc = 0;

    if (streq(action, _T("Property")) ||
	    streq(action, _T("property"))) {
	// Print the current value of the named property.
	int i;
	CCS str;

	for (i = 0; i < argc; i++) {
	    if (vb_bitmatch(VB_STD)) {
		printf(_T("%s="), argv[i]);
	    }
	    if ((str = prop_value_from_name(argv[i]))) {
		printf(_T("%s\n"), str);
	    } else {
		printf(_T("\n"));
	    }
	}
    } else if (streq(action, _T("Substitute")) ||
	    streq(action, _T("substitute"))) {
	// Apply standard %-substitutions to the input string.
	int i;

	for (i = 0; i < argc; i++) {
	    TCHAR subsbuf[8192];

	    util_substitute_params(argv[i], subsbuf, charlen(subsbuf));
	    printf(_T("%s\n"), subsbuf);
	}
    } else if (streq(action, _T("hash-object"))) {
	int write_flag = 0;
	CCS dcode = NULL;
	ps_o ps;

	for (bsd_getopt_reset(); argv && *argv; ) {
	    int c;

	    // *INDENT-OFF*
	    static CS short_opts = _T("+s:w");
	    static struct option long_opts[] = {
		{_T("sha1"),		required_argument, NULL, 's'},
		{_T("write"),		no_argument,	   NULL, 'w'},
		{0,			0,		   NULL,  0 },
	    };
	    // *INDENT-ON*

	    c = bsd_getopt(argc + 1, argv - 1, short_opts, long_opts, NULL);
	    if (c == -1 || c == '?') {
		break;
	    }

	    switch (c) {
		case 's':
		    dcode = putil_strdup(bsd_optarg);
		    break;
		case 'w':
		    write_flag = 1;
		    break;
		default:
		    break;
	    }
	}
	argc -= (bsd_optind - 1);
	argv += (bsd_optind - 1);

	ps = ps_newFromPath(*argv);
	if (dcode) {
	    ps_set_dcode(ps, dcode);
	} else if (ps_stat(ps, 1)) {
	    putil_syserr(2, *argv);
	}

	if (write_flag) {
	    git_store_blob(ps);
	} else {
	    printf("%s\n", ps_get_dcode(ps));
	}

	ps_destroy(ps);
	putil_free(dcode);
    } else if (streq(action, _T("Stat")) || streq(action, _T("stat"))) {
	// print vital statistics for specified files.
	int long_flag = 0, short_flag = 0, deref_flag = 0;
	CCS path;

	for (bsd_getopt_reset(); argv && *argv; ) {
	    int c;

	    // *INDENT-OFF*
	    static CS short_opts = _T("+alsL");
	    static struct option long_opts[] = {
		{_T("absolute-paths"),	no_argument,	   NULL, 'a'},
		{_T("long"),		no_argument,	   NULL, 'l'},
		{_T("short"),		no_argument,	   NULL, 's'},
		{_T("dereference"),	no_argument,	   NULL, 'L'},
		{0,			0,		   NULL,  0 },
	    };
	    // *INDENT-ON*

	    c = bsd_getopt(argc + 1, argv - 1, short_opts, long_opts, NULL);
	    if (c == -1 || c == '?') {
		break;
	    }

	    switch (c) {
		case 'a':
		    prop_override_true(P_ABSOLUTE_PATHS);
		    break;
		case 'l':
		    long_flag = 1;
		    break;
		case 's':
		    short_flag = 1;
		    break;
		case 'L':
		    deref_flag = 1;
		    break;
		default:
		    break;
	    }
	}
	argc -= (bsd_optind - 1);
	argv += (bsd_optind - 1);

	// Let the user see the data we'd derive for the specified local file.
	for (; *argv; argv++) {
	    struct __stat64 stbuf;
	    int statrc;
	    ps_o ps;
	    TCHAR buf[PATH_MAX + 256];

	    ps = ps_newFromPath(*argv);
	    path = ps_get_abs(ps);

	    if (deref_flag) {
		statrc = _tstat64(path, &stbuf);
	    } else {
		statrc = _tlstat64(path, &stbuf);
	    }

	    if (statrc) {
		putil_syserr(0, path);
		ps_destroy(ps);
		continue;
	    } else if (!S_ISREG(stbuf.st_mode)
#if !defined(_WIN32)
		       && !S_ISLNK(stbuf.st_mode)
#endif	/*_WIN32*/
		) {
		putil_warn(_T("%s: not a regular file"), path);
		ps_destroy(ps);
		continue;
	    }

	    if (ps_stat(ps, 1)) {
		rc = 1;
	    } else {
		ps_format_user(ps, long_flag, short_flag, buf, charlen(buf));
		_fputts(buf, stdout);
	    }

	    ps_destroy(ps);
	}
    } else if (streq(action, _T("about"))) {
	int short_flag = 0;

	for (bsd_getopt_reset(); argv && *argv; ) {
	    int c;

	    // *INDENT-OFF*
	    static CS short_opts = _T("+s");
	    static struct option long_opts[] = {
		{_T("short"),		no_argument,	   NULL, 's'},
		{0,			0,		   NULL,  0 },
	    };
	    // *INDENT-ON*

	    c = bsd_getopt(argc + 1, argv - 1, short_opts, long_opts, NULL);
	    if (c == -1 || c == '?') {
		break;
	    }

	    switch (c) {
		case 's':
		    short_flag = 1;
		    break;
		default:
		    break;
	    }
	}

	if (!short_flag) {
	    print_license("Audited Objects", self_license);

	    fputs("This software incorporates all or parts ", stdout);
	    fputs("of the following, whose\ncontributions ", stdout);
	    fputs("are gratefully acknowledged:\n\n", stdout);

	    if ((rc = http_ping())) {
		about_client();
	    } else {
		rc = http_action(action, argv, 0);
	    }
	}
	_print_version(0);
    } else if (*argv && (streq(*argv, "-h") || streq(*argv, "--help"))) {
	// Always go to the server for help unless it's a pure client-side
	// action. That's why this is midway down - pure client actions
	// are above, anything requiring server interaction is below.
	rc = http_action(action, argv, 0);
    } else if (streq(action, _T("label"))) {
	CS const *targv;

	for (bsd_getopt_reset(); argv && *argv; ) {
	    int c;

	    // *INDENT-OFF*
	    static CS short_opts = _T("+sli:p:");
	    static struct option long_opts[] = {
		{_T("long"),		no_argument,	   NULL, 'l'},
		{_T("short"),		no_argument,	   NULL, 's'},
		{_T("id"),		required_argument, NULL, 'i'},
		{_T("project-name"),	required_argument, NULL, 'p'},
		{0,			0,		   NULL,  0 },
	    };
	    // *INDENT-ON*

	    c = bsd_getopt(argc + 1, argv - 1, short_opts, long_opts, NULL);
	    if (c == -1 || c == '?') {
		break;
	    }

	    switch (c) {
		default:
		    break;
	    }
	}
	targv = argv + (bsd_optind - 1);

	if (!*targv) {
	    putil_die("no label specified");
	} else if (targv[1]) {
	    putil_die("conflicting labels specified");
	} else {
	    if (http_label(*targv, argv)) {
		rc = 2;
	    }
	}
    } else if (!_tcsncmp(action, _T("namestate"), 4)) {
	CS const *targv;
	CCS name;

	// HACK - skip past flags to reach files.
	for (targv = argv; targv[0][0] == '-' && targv[1]; targv++) {
	    if (streq(*targv, "-p") || streq(*targv, "-i")) {
		targv++;
	    }
	}
	name = *targv++;
	if (!name) {
	    putil_die("no label specified");
	}
	if (!*targv) {
	    putil_die("no pathnames specified");
	}

	for (; *targv; targv++) {
	    if (_name_pathstate(name, *targv, argv)) {
		rc = 2;
	    }
	}
    } else if (!_tcscmp(action, _T("Admin"))) {
	if (argv[0] && streq(argv[0], _T("restart"))) {
	    rc = http_restart();
	} else {
	    rc = http_action(action, argv, 0);
	}
    } else {
	if (streq(action, _T("help")) && !argv[0]) {
	    const char *fmt = "%-12s - %s\n";

	    // Here we document only actions which are PURELY client-side,
	    // aside from the "run" variants.
	    // If they have any server part, e.g. "label" or "ADMIN",
	    // then let the server document them.
	    printf(fmt, "run", "run and audit the specified command");
	    printf(fmt, "make", "shorthand for 'run make'");
	    printf(fmt, "stat", "print vital statistics for specified files");
	    printf(fmt, "property", "print the value of the named property");
	    printf(fmt, "substitute",
		"apply standard %-substitutions to the input string");
	    printf("\n");
	}

	if (!_tcsncmp(action, _T("lsb"), 3)) {
	    rc = http_action(action, argv, 1);
	} else {
	    rc = http_action(action, argv, 0);
	}
    }

    return rc;
}

// Do a quiet "make clean" or "ant clean" before kicking off build.
// This is useful for debug/devel work. Note that this time is
// deliberately not counted (i.e. we do not set StartTime until
// after it runs). This can lead to confusion when compared with
// actual wall-clock time but the clean really should not count
// when gathering timing statistics.
static void
_make_clean(void)
{
    TCHAR clean[1024];

#if defined(_WIN32)
    if (!_taccess("NMakefile", R_OK)) {
	snprintf(clean, sizeof(clean) - 1,
	    "nmake /nologo /s /f NMakefile clean >%s", DEVNULL);
    } else if (!_taccess("build.xml", R_OK)) {
	snprintf(clean, sizeof(clean) - 1, "ant -q clean >%s", DEVNULL);
    } else {
	snprintf(clean, sizeof(clean) - 1, "nmake /nologo /s clean >%s",
	    DEVNULL);
    }
#else	/*_WIN32*/
    if (!_taccess("Makefile", R_OK) || !_taccess("makefile", R_OK)) {
	snprintf(clean, sizeof(clean) - 1, "make -s clean >%s", DEVNULL);
    } else if (!_taccess("build.xml", R_OK)) {
	snprintf(clean, sizeof(clean) - 1, "ant -q clean >%s", DEVNULL);
    } else {
	snprintf(clean, sizeof(clean) - 1, "make -s clean >%s", DEVNULL);
    }
#endif	/*_WIN32*/

    if (QuietMode) {
	strcat(clean, " 2>&1");
    } else {
	fprintf(stderr, "+ %s\n", clean);
    }

    if (system(clean)) {
	exit(2);
    }
}

/// This is the entry point for the client program.
/// @param[in] argc     the number of arguments
/// @param[in] argv     the classic argument vector taken by every main routine
/// @return the exit code of the process
int
main(int argc, CS const *argv)
{
    CCS exe;
    CCS action;
    CCS pager = NULL;
    CS dscript = NULL;
    int proplevel = -1;
    int no_server = 0;
    int rc = 0;

    // Figure out the full path to the current executable program.
    // Should be done asap, before anything happens to argv.
    if (!(exe = putil_getexecpath())) {
	putil_die(_T("unable to determine path to argv[0]\n"));
    }

#if defined(_WIN32)
    if (putil_getenv("DEBUGBREAK")) {
	DebugBreak();
    }
#endif	/*_WIN32*/

    // Initialize verbosity. Should be done before most
    // other initializations since they may use verbosity.
    vb_init();
    atexit(vb_fini);

    // Initialize the properties database and, at the same time,
    // arrange for it to be finalized at exit time. Doing it this
    // way ensures that it will be finalized last, meaning that
    // other exit and error handling routines will have access
    // to properties until the bitter end. Really, the only reason
    // to explicitly free data right before exit like this is
    // to avoid spurious warnings from memory leak detectors like
    // valgrind and make real problems more obvious.
    prop_init(_T(APPLICATION_NAME));
    atexit(prop_fini);
    // This has been known to cause core dumps on Linux. I think the
    // problem is since fixed but since it does no real good, as noted,
    // I'm leaving it off for now.
    //atexit(prop_unexport_all);

    // Load properties from the documented sequence of prop files and
    // the environment.
    prefs_init(exe, PROP_EXT, NULL);

    // Initialize the hash-code generation.
    code_init();

    // Parse the command line up to the first unrecognized item.
    // E.g. given "command -flag1 -flag2 arg1 -flag3 -flag4" we parse
    // only -flag1 and -flag2.
    for (bsd_getopt_reset(); ;) {
	int c;

	// *INDENT-OFF*
	static CS short_opts =
	    _T("+1adhl:mo:p:qrs:uv::wxACDEF:GH::I:LM:O:PQRSTUV:W:XY");
	static struct option long_opts[] = {
	    {_T("oneshell"),		no_argument,	   NULL, '1'},
	    {_T("absolute-paths"),	no_argument,	   NULL, 'a'},
	    {_T("agg-level"),		required_argument, NULL, LF('A','G')},
	    {_T("audit-only"),		no_argument,	   NULL, 'A'},
	    {_T("make-clean"),		no_argument,	   NULL, 'C'},
	    {_T("download-only"),	no_argument,	   NULL, 'd'},
	    {_T("download-silent"),	no_argument,	   NULL, 'D'},
	    {_T("dtrace"),		required_argument, NULL, LF('D','T')},
	    {_T("client-platform"),	required_argument, NULL, LF('C','P')},
	    {_T("error-strict"),	no_argument,	   NULL, 'E'},
	    {_T("make-file"),		required_argument, NULL, 'F'},
	    {_T("git"),			no_argument,	   NULL, 'G'},
	    {_T("help"),		no_argument,	   NULL, 'h'},
	    {_T("Help"),		optional_argument, NULL, 'H'},
	    {_T("properties"),		optional_argument, NULL, LF('P','*')},
	    {_T("identity-hash"),	required_argument, NULL, 'I'},
	    {_T("log-file"),		required_argument, NULL, 'l'},
	    {_T("log-file-temp"),	no_argument,	   NULL, 'L'},
	    {_T("make-depends"),	optional_argument, NULL, 'M'},
	    {_T("mem-debug"),		optional_argument, NULL, LF('D','M')},
	    {_T("members-only"),	no_argument,	   NULL, 'm'},
	    {_T("output-file"),		required_argument, NULL, 'o'},
	    {_T("Output-file"),		required_argument, NULL, 'O'},
	    {_T("project-name"),	required_argument, NULL, 'p'},
	    {_T("pager"),		no_argument,	   NULL, 'P'},
	    {_T("profile"),		no_argument,	   NULL, LF('P','%')},
	    {_T("quiet"),		no_argument,	   NULL, 'q'},
	    {_T("extra-quiet"),		no_argument,	   NULL, 'Q'},
	    {_T("leave-roadmap"),	no_argument,	   NULL, 'r'},
	    {_T("reuse-roadmap"),	no_argument,       NULL, 'R'},
	    {_T("restart"),		no_argument,	   NULL, LF('R','L')},
	    {_T("server"),		required_argument, NULL, 's'},
	    {_T("strict"),		no_argument,	   NULL, 'S'},
	    {_T("print-elapsed"),	no_argument,	   NULL, 'T'},
	    {_T("upload-only"),		no_argument,	   NULL, 'u'},
	    {_T("uncompressed-transfers"), no_argument,	   NULL, 'U'},
	    {_T("verbosity"),		optional_argument, NULL, 'v'},
	    {_T("local-verbosity"),	required_argument, NULL, 'V'},
	    {_T("version"),		optional_argument, NULL, LF('v','n')},
	    {_T("why"),			no_argument,	   NULL, 'w'},
	    {_T("WFlag"),		required_argument, NULL, 'W'},
	    {_T("exec-verbosity"),	required_argument, NULL, 'x'},
	    {_T("execute-only"),	no_argument,	   NULL, 'X'},
	    {_T("synchronous-transfers"), no_argument,	   NULL, 'Y'},
	    {0,				0,		   NULL,  0 },
	};
	// *INDENT-ON*

	c = bsd_getopt(argc, argv, short_opts, long_opts, NULL);
	if (c == -1) {
	    break;
	}

	switch (c) {

	    case '1':
		putil_putenv(_T("MAKEFLAGS=--eval .ONESHELL:"));
		break;

	    case 'a':
		prop_override_true(P_ABSOLUTE_PATHS);
		break;

	    case LF('A','G'):
		prop_override_str(P_AGGREGATION_STYLE, bsd_optarg);
		break;

	    case 'A':
		prop_override_true(P_AUDIT_ONLY);
		break;

	    // Development hack - allows an automatic "make clean" which
	    // is not audited or counted in elapsed time.
	    case 'C':
		_make_clean();
		break;

	    case LF('C', 'P'):
		prop_override_str(P_CLIENT_PLATFORM, bsd_optarg);
		break;

	    // Undocumented - used to debug downloads without modifying state.
	    // We may not need this forever.
	    case 'D':
		prop_unset(P_DOWNLOAD_ONLY, 0);
		prop_put_ulong(P_DOWNLOAD_ONLY, 2);
		break;

	    case 'd':
		prop_override_true(P_DOWNLOAD_ONLY);
		break;

	    case LF('D', 'T'):
		dscript = bsd_optarg;
		break;

	    case 'E':
		prop_put_long(P_STRICT_ERROR, 1);
		break;

	    case 'I':
		prop_override_str(P_IDENTITY_HASH, bsd_optarg);
		break;

	    case 'L':
		{
		    TCHAR buf[1024];

		    _sntprintf(buf, 1024, "%s.%lu.log",
			       prop_get_str(P_APP), (unsigned long)getpid());
		    prop_override_str(P_LOG_FILE, buf);
		    prop_override_ulong(P_LOG_FILE_TEMP, 1);
		}
		break;

	    case 'l':
		prop_override_str(P_LOG_FILE, bsd_optarg);
		break;

	    case 'F':
		prop_override_str(P_MAKE_FILE, bsd_optarg);
		break;

	    case 'G':
		prop_override_true(P_GIT);
		no_server = 1;
		break;

	    case 'M':
		if (!bsd_optarg || !_tcscmp(bsd_optarg, _T("D"))) {
		    prop_override_str(P_MAKE_DEPENDS, _T("d"));
		} else {
		    prop_override_str(P_MAKE_DEPENDS, bsd_optarg);
		}
		break;

	    case 'm':
		prop_override_true(P_MEMBERS_ONLY);
		break;

	    case 'o':
		no_server = 1;
	    case 'O':
		if (!bsd_optarg || !_tcscmp(bsd_optarg, _T("@"))) {
		    prop_override_str(P_OUTPUT_FILE, DEVNULL);
		} else {
		    prop_override_str(P_OUTPUT_FILE, bsd_optarg);
		}
		break;

	    case 'p':
		prop_override_str(P_PROJECT_NAME, bsd_optarg);
		break;

	    case 'P':
		if (!(pager = _tgetenv(_T("PAGER")))) {
		    pager = _T("less");
		}
		break;

		// Generate runtime linker profile output. Works on
		// Solaris and Linux at least.
	    case LF('P', '%'):
#if !defined(_WIN32)
		putil_putenv(_T("LD_PROFILE=") _T(AUDITOR));
#endif	/*_WIN32*/
		break;

	    case 'Q':
		prop_override_str(P_SERVER_LOG_LEVEL, "WARN");
	    case 'q':
		QuietMode = 1;
		vb_addstr("-");
		break;

	    case 'R':
		prop_override_true(P_REUSE_ROADMAP);
		prop_unset(P_DOWNLOAD_ONLY, 0);
		prop_put_ulong(P_DOWNLOAD_ONLY, 2);
	    case 'r':
		prop_override_str(P_ROADMAPFILE, ROADMAP_DEFAULT_NAME);
		prop_override_true(P_LEAVE_ROADMAP);
		break;

	    case LF('R', 'L'):
		if ((rc = http_restart())) {
		    exit(rc);
		}
		break;

	    case 's':
		prop_override_str(P_SERVER, bsd_optarg);
		break;

	    case 'S':
		prop_put_ulong(P_STRICT, 1);
		prop_put_ulong(P_STRICT_DOWNLOAD, 1);
		prop_put_ulong(P_STRICT_UPLOAD, 1);
		break;

	    case 'T':
		prop_unset(P_PRINT_ELAPSED, 1);
		prop_put_str(P_PRINT_ELAPSED, _T("-1"));
		break;

	    case 'u':
		prop_override_ulong(P_UPLOAD_ONLY, 1);
		break;

	    case 'U':
		prop_override_ulong(P_UNCOMPRESSED_TRANSFERS, 1);
		break;

	    case 'V':		// keeps verbosity out of auditor
		prop_unexport(P_VERBOSITY, 1);
	    case 'v':
		vb_addstr(bsd_optarg);
		break;

	    case LF('D', 'M'):
#if defined(sun)
		if (bsd_optarg && *bsd_optarg == 'w') {
		    putil_putenv("LD_PRELOAD=watchmalloc.so.1");
		    if (!putil_getenv("MALLOC_DEBUG")) {
			putil_putenv("MALLOC_DEBUG=WATCH");
		    }
		    printf("MALLOC_DEBUG=%s\n", putil_getenv("MALLOC_DEBUG"));
		} else {
		    putil_putenv("LD_PRELOAD=/lib/libumem.so");
		    if (!putil_getenv("UMEM_DEBUG")) {
			putil_putenv("UMEM_DEBUG=default,verbose");
		    }
		    printf("UMEM_DEBUG=%s\n", putil_getenv("UMEM_DEBUG"));
		}
		if (putil_getenv("LD_PRELOAD")) {
		    printf("LD_PRELOAD=%s\n", putil_getenv("LD_PRELOAD"));
		}
#elif defined(__APPLE__)
		putil_putenv("MallocScribble=1");
		putil_putenv("MallocPreScribble=1");
		putil_putenv("MallocBadFreeAbort=1");
		putil_putenv("MallocCheckHeapAbort=1");
		//putil_putenv("MallocStackLogging=1");
		//putil_putenv("MallocStackLoggingNoCompact=1");
		//putil_putenv("MallocCheckHeapStart=1000");
		//putil_putenv("MallocCheckHeapEach=1000");
		//putil_putenv("MallocCheckHeapSleep=-200");
		//(void)system("env | grep '^Malloc'");
#else	/*__APPLE__*/
		putil_die(_T("no malloc debugger implemented"));
#endif	/*__APPLE__*/
		break;

	    case LF('v', 'n'):
		_print_version(bsd_optarg && bsd_optarg[0] == '+');
		exit(0);
		break;

	    case 'w':
		vb_addbit(VB_WHY);
		break;

	    case 'W':
		// This is reserved for extension flags a la gcc.
		// E.g. "-Wm,-v,varfile" would pass "-v varfile" to the
		// makefile generator.
		{
		    CCS old;
		    CS str;

		    if ((old = prop_get_str(P_WFLAG))) {
			asprintf(&str, "%s\n%s", old, bsd_optarg);
			prop_override_str(P_WFLAG, str);
			putil_free(str);
		    } else {
			prop_put_str(P_WFLAG, bsd_optarg);
		    }
		}
		break;

	    case 'x':
		vb_addbit(VB_EXEC);
		break;

	    case 'X':
		prop_override_true(P_EXECUTE_ONLY);
		prop_put_ulong(P_PRINT_ELAPSED, 1);
		prop_unset(P_SERVER, 1);
		break;

	    case 'Y':
		prop_override_true(P_SYNCHRONOUS_TRANSFERS);
		break;

	    case 'h':
		_usage(0);
		break;

	    case LF('P','*'):	// alias to --Help for mnemonic reasons
	    case 'H':
		// Put off printing properties till they're all filled in.
		if (bsd_optarg && *bsd_optarg == '+') {
		    proplevel = 1;
		} else {
		    proplevel = 0;
		}
		break;

	    default:
		_usage(1);
	}
    }
    argc -= bsd_optind;
    argv += bsd_optind;

    // The command we've been asked to invoke.
    if ((action = argv[0])) {
	// Hack for typing ease: "make" is a synonym for "run make".
	// Similarly, "ant" is a synonym for "run ant".
	// And any word containing a / must be a program to run.
	if (strchr(action, '/') || strchr(action, '\\')
	    || _tcsstr(action, _T("make"))
	    || !_tcscmp(action, _T("sh"))
	    || !_tcsicmp(action, _T("vcbuild"))		// Windows only
	    || !_tcsicmp(action, _T("msbuild"))		// Windows only
	    || !_tcscmp(action, _T("ant"))) {
	    action = _T("run");
	} else {
	    argv++;
	    argc--;
	}
    } else {
	// The default command ...
	action = _T("help");
    }

    // Default project name is the name of the dir containing the project
    // config file, with any numerical extension following a '-' removed.
    if (!prop_has_value(P_PROJECT_NAME)) {
	CCS pbase;
	CS pjname, ptr, dash;

	pbase = prop_get_str(P_BASE_DIR);
	if (pbase && (pjname = putil_basename(pbase)) && *pjname) {
	    ptr = putil_strdup(pjname);
	    if ((dash = _tcschr(ptr, '-')) && ISDIGIT(dash[1])) {
		*dash = '\0';
	    }
	    prop_put_str(P_PROJECT_NAME, ptr);
	    putil_free(ptr);
	}
    }

    // For convenience, the -o flag suppresses server interaction along
    // with specifying an output file.
    if (no_server) {
	prop_unset(P_SERVER, 1);
    }

    // Recycling would break full-makefile generation.
    if (prop_has_value(P_MAKE_FILE) && !prop_has_value(P_MAKE_DEPENDS)) {
	prop_override_ulong(P_UPLOAD_ONLY, 1);
    }

    // Optionally run help output through a pager.
#if !defined(_WIN32)
    if (pager ||
	(streq(action, _T("man")) && (pager = prop_get_str(P_DOC_PAGER)))) {
	int pfd[2];
	pid_t pid;

	// Allow indirection through standard $PAGER EV.
	if (*pager == '$') {
	    if (!(pager = getenv(pager + 1))) {
		pager = prop_get_str(P_DOC_PAGER);
	    }
	}

	fflush(NULL);

	if (pipe(pfd) < 0) {
	    putil_syserr(2, _T("pipe"));
	}

	// Note that the *parent* process becomes the pager.
	if ((pid = fork()) == 0) {
	    // Child
	    close(pfd[0]);
	    if (pfd[1] != STDOUT_FILENO) {
		dup2(pfd[1], STDOUT_FILENO);
		close(pfd[1]);
	    }
	} else if (pid > 0) {
	    // Parent
	    close(pfd[1]);
	    if (pfd[0] != STDIN_FILENO) {
		dup2(pfd[0], STDIN_FILENO);
		close(pfd[0]);
	    }

	    execlp(pager, pager, (char *)0);
	    putil_syserr(0, pager);
	    _exit(2);
	} else {
	    putil_syserr(2, _T("fork"));
	}
    }
#endif	/*_WIN32*/

    // For debugging convenience allow server=nnnn to mean "localhost:nnnn"
    // and server=foo to mean "foo:8080" since those are the most common.
    if (prop_has_value(P_SERVER)) {
	const char *svr;

	svr = prop_get_strA(P_SERVER);
	if (!strchr(svr, ':')) {
	    char buf[256];

	    if (isdigit((int)*svr)) {
		snprintf(buf, sizeof(buf) - 1, "localhost:%s", svr);
	    } else {
		snprintf(buf, sizeof(buf) - 1, "%s:8080", svr);
	    }
	    prop_override_str(P_SERVER, buf);
	}
    }

    // Diagnostic: dump properties and quit.
    if (proplevel >= 0) {
	prop_help(proplevel, vb_bitmatch(VB_STD), exe);
	exit(0);
    }

    http_init();

    // Arrange for the elapsed time to be printed out at exit time
    // if requested by user property. The property is not a boolean
    // but a mimimum number of seconds which must elapse in order
    // to trigger output.
    if ((ElapsedMinimum = prop_get_long(P_PRINT_ELAPSED))) {
	StartTime = time(NULL);
	atexit(_print_elapsed_time);
    }

    if (streq(action, _T("run"))) {
	TCHAR cwd[PATH_MAX];
	TCHAR logbuf[PATH_MAX];
	CCS rmap, logfile;

	if (!argv || !*argv) {
	    _usage(1);
	}

	// The auditor ignores anything it considers to be a temp
	// file, which can cause terribly confusing behavior if
	// run on a project rooted in (say) /tmp. It's a natural
	// tendency for someone setting up a quick test case to
	// put it in /tmp and then spend hours trying to figure
	// out why it's behaving strangely. To help avoid this
	// we disallow the auditor from running when the CWD is
	// one we consider a repository for temp files.
	if (util_get_cwd(cwd, charlen(cwd))) {
	    if (util_is_tmp(cwd) && !prop_is_true(P_EXECUTE_ONLY)) {
		putil_die(_T("illegal tmp working directory: %s"), cwd);
	    }
	} else {
	    putil_syserr(2, _T("util_get_cwd()"));
	}

	// These values are pre-exported to reserve env-block space.
	// To not do so would risk a dangerous realloc of the 'environ'
	// pointer within an unsuspecting host process.

	// Reserve a block of env space for the current cmd depth.
	prop_put_ulong(P_DEPTH, 0);

	// Reserve a block of env space for the parent cmd id.
	// Seed it with our pid.
	prop_put_ulong(P_PCMDID, getpid());

	// Reserve a block of env space for the parent cmd code.
	prop_put_str(P_PCCODE, CSV_NULL_FIELD);

	// This needs to be present in the env from the start even when off.
	// But being a boolean it only needs room for one digit.
	prop_put_ulong(P_AGGREGATED_SUBCMD, 0);

	// Allow the cmd to be preceded by EV's in the manner of the shell.
	while (_tcschr(*argv, '=')) {
	    putil_putenv(*argv++);
	}

	// Determine whether the user requested a log file and
	// apply the %u, %p, etc. formats to it if so.
	if ((logfile = prop_get_str(P_LOG_FILE))) {
	    if (util_substitute_params(logfile, logbuf, charlen(logbuf))) {
		logfile = logbuf;
	    }
	    // In case it's present but unwriteable.
	    (void)unlink(logfile);
	}

	// Work out the name of the roadmap file and store the result.
	if ((rmap = prop_get_str(P_ROADMAPFILE))) {
	    if (!putil_is_absolute(rmap)) {
		TCHAR rbuf[PATH_MAX];

		if ((rmap = putil_realpath(rmap, rbuf, charlen(rbuf), 1))) {
		    prop_override_str(P_ROADMAPFILE, rbuf);
		} else {
		    putil_syserr(2, prop_get_str(P_ROADMAPFILE));
		}
	    }
	} else if (prop_has_value(P_SERVER)) {
	    rmap = tempnam(NULL, "rmap.");
	    prop_put_str(P_ROADMAPFILE, rmap);
	    putil_free(rmap);
	}

	if (prop_has_value(P_SERVER)) {
	    // Get the new roadmap.
	    if (!prop_is_true(P_REUSE_ROADMAP) &&
		    !prop_is_true(P_UPLOAD_ONLY)) {
		mon_get_roadmap();
	    }

	    // Start a session. We do this *after* getting the roadmap
	    // to avoid session expiration during roadmap-getting.
	    // That would only be a risk for a very slow roadmap or a
	    // very fast session expiration but it could happen.
	    if ((rc = mon_begin_session())) {
		putil_die(_T("can't get a session at %s"),
			  prop_get_str(P_SERVER));
	    }
	}

	// For systems supporting DTrace: run the cmd with the specified
	// dtrace script.
	if (dscript) {
	    CS *dargv;
	    
	    dargv = putil_malloc(sizeof(*dargv) * 6);
	    dargv[0] = _T("dtrace");
	    dargv[1] = _T("-s");
	    dargv[2] = dscript;
	    dargv[3] = _T("-c");
	    dargv[4] = util_requote(argv);
	    dargv[5] = NULL;
	    argv = dargv;
	    fprintf(stderr, "+ dtrace -s %s -c '%s'\n", dscript, argv[4]);
	}

	make_init(exe);

	if (prop_is_true(P_GIT)) {
	    git_init(exe);
	}

	// RUN AND AUDIT THE COMMAND.
	rc = run_cmd(exe, (CS *)argv, logfile);

	if (prop_is_true(P_GIT)) {
	    git_fini();
	}

	make_fini();

	// If using a temp logfile, remove it now.
	if (prop_is_true(P_LOG_FILE_TEMP)) {
	    unlink(prop_get_str(P_LOG_FILE));
	}
    } else if (streq(action, _T("roadmap"))) {
	// Useful while testing roadmaps. May be dispensed with later.
	prop_override_str(P_ROADMAPFILE, ROADMAP_DEFAULT_NAME);
	prop_override_true(P_LEAVE_ROADMAP);
	mon_get_roadmap();
    } else if (streq(action, _T("shop"))) {
	int cflag = 0;
	int gflag = 0;
	ca_o ca;
	TCHAR rwdbuf[PATH_MAX];

	// This special action is useful for internal tests of shopping
	// capabilities without changing server state, using a saved
	// roadmap file.

	while (*argv && **argv == '-') {
	    if (!_tcscmp(*argv, _T("-C"))) {
		// Work with a predetermined cmd index rather than a cmd line.
		cflag = 1;
	    } else if (!_tcscmp(*argv, _T("-G"))) {
		// Actually get files (default is to not change local state).
		gflag = 1;
	    }
	    argv++;
	}

	if (!argv[0]) {
	    putil_die("Usage: shop [-G] -C index | cmd...");
	}

	// Assume the pre-existence of a roadmap with the default name.
	prop_override_str(P_ROADMAPFILE, ROADMAP_DEFAULT_NAME);
	prop_override_true(P_LEAVE_ROADMAP);

	ca = ca_new();
	ca_set_pccode(ca, CSV_NULL_FIELD);
	ca_set_prog(ca, prop_get_str(P_PROGNAME));
	ca_set_host(ca, "localhost");
	ca_set_cmdid(ca, getpid());
	ca_set_pcmdid(ca, prop_get_ulong(P_PCMDID));
	ca_set_rwd(ca, util_get_rwd(rwdbuf, charlen(rwdbuf)));
	ca_set_started(ca, 1);

	// We almost always want these while debugging...
	vb_addbit(VB_SHOP);
	vb_addbit(VB_WHY);

	shop_init();
	if (cflag) {
	    rc = shop(ca, argv[0], gflag);
	} else {
	    CCS cmdline;

	    cmdline = util_requote(argv);
	    ca_set_line(ca, cmdline);
	    putil_free(cmdline);
	    rc = shop(ca, NULL, gflag);
	}
	shop_fini();
    } else {
	rc = do_action(action, argc, argv);
    }

    code_fini();

    http_fini();

    return rc;
}
