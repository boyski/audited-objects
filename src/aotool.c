// Copyright (c) 2002-2011 David Boyce.  All rights reserved.

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
#include "git2.h"
#include "zlib.h"

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
	fprintf(fp, fmt, flag, desc);
    } else {
	char buf[1024];

	snprintf(buf, charlen(buf), "%s [=%s]",
	    prop_desc(prop), prop_to_name(prop));
	fprintf(fp, fmt, flag, buf);
    }
}

// Internal service routine.
static void
_usage(int rc)
{
    FILE *fp;
    CCS fmt = "   %-15s %s\n";
    CCS prog;

    prog = prop_get_str(P_PROGNAME);

    fp = rc ? stderr : stdout;

    fprintf(fp, "USAGE: %s [<options>]", prog);
    fprintf(fp, " { make | run <prog> | <action> } [args...]\n");
    fprintf(fp, "FLAGS:\n");

    fprintf(fp, fmt, "-h", "Print this usage summary");
    fprintf(fp, fmt, "-H", "Print current properties");
    _usg_line(fp, fmt, "-a", P_ABSOLUTE_PATHS, NULL);
    fprintf(fp, fmt, "-C dir", "Change to dir before doing anything");
    _usg_line(fp, fmt, "-d", P_DOWNLOAD_ONLY, NULL);
    _usg_line(fp, fmt, "-F file", P_MAKE_FILE, NULL);
#if !defined(_WIN32)
    _usg_line(fp, fmt, "-l file", P_LOG_FILE, NULL);
    _usg_line(fp, fmt, "-L", P_LOG_FILE_TEMP, NULL);
#endif	/*_WIN32*/
    _usg_line(fp, fmt, "-MD", P_MAKE_DEPENDS, NULL);
    _usg_line(fp, fmt, "-m", P_MEMBERS_ONLY, NULL);
    _usg_line(fp, fmt, "-o file", P_OUTPUT_FILE, NULL);
    _usg_line(fp, fmt, "-p name", P_PROJECT_NAME, NULL);
    fprintf(fp, fmt, "-q", "Quiet mode: suppress verbosity");
    _usg_line(fp, fmt, "-s host:port", P_SERVER, NULL);
    _usg_line(fp, fmt, "-t", P_PRINT_ELAPSED, NULL);
    _usg_line(fp, fmt, "-u", P_UPLOAD_ONLY, NULL);
    fprintf(fp, fmt, "-vXX,YY,ZZ",
	"Set verbosity flags (use -v? to see choices");
    fprintf(fp, fmt, "--version", "Print version and exit");
    fprintf(fp, fmt, "-w", "Explain why commands cannot be recycled");
    fprintf(fp, fmt, "-Wm,flag,value",
	"Pass -flag=value to makefile generator");
    fprintf(fp, fmt, "-x", "Print each command line as executed");
    _usg_line(fp, fmt, "-X", P_EXECUTE_ONLY, NULL);

    fprintf(fp, "EXAMPLES:\n");
    fprintf(fp, "   %s help\n", prog);
    fprintf(fp, "   %s ping\n", prog);
    fprintf(fp, "   %s -o@ -F Makefile.new make -s clean all\n", prog);
    fprintf(fp, "   %s make clean all\n", prog);
    fprintf(fp, "   %s lsbuilds -s\n", prog);

    putil_exit(rc);
}

// Internal service routine. TODO Not yet re-enabled after persistence
// changes - this is a stub left around pending server/schema work.
static int
_name_pathstate(CCS name, CCS path, CS const *argv)
{
    struct __stat64 stbuf;
    ps_o ps;
    CCS psbuf;
    int rc = 0;

    // If we can't resolve the file vs the cwd, try vs the rwd.
    if (stat64(path, &stbuf)) {
	if (putil_is_absolute(path)) {
	    putil_syserr(0, path);
	    return 2;
	} else {
	    CS abspath;

	    asprintf(&abspath, "%s/%s", prop_get_str(P_BASE_DIR), path);
	    if (stat64(abspath, &stbuf)) {
		putil_syserr(0, path);	// NOT abspath
		putil_free(abspath);
		return 2;
	    } else {
		ps = ps_newFromPath(abspath);
		putil_free(abspath);
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
	ps_destroy(ps);
	return 2;
    }

    psbuf = ps_toCSVString(ps);

    rc = http_namestate(name, psbuf, argv);

    putil_free(psbuf);

    ps_destroy(ps);

    return rc;
}

/// Prints the version of this application to stdout.
/// @param[in] full      if true, print versions of all 3rd-party libs too
static void
_print_version(int full)
{
    printf("%s=%s\n",
	     APPLICATION_NAME, APPLICATION_VERSION);

    if (full) {
	fputs(putil_builton(), stdout);
#if defined(_WIN32) && defined(_DEBUG)
	fputs(" (DEBUG)", stdout);
#endif	/*_DEBUG*/
	fputs("\n", stdout);
	printf("libcurl=%s\n", curl_version());
	printf("pcre=%s\n", pcre_version());
	printf("libgit2=%s\n", LIBGIT2_VERSION);
	printf("zlib=%s\n", (const char *)zlibVersion());
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

    if (streq(action, "Property") ||
	    streq(action, "property")) {
	// Print the current value of the named property.
	int i;
	CCS str;

	for (i = 0; i < argc; i++) {
	    if (vb_bitmatch(VB_STD)) {
		printf("%s=", argv[i]);
	    }
	    if ((str = prop_value_from_name(argv[i]))) {
		printf("%s\n", str);
	    } else {
		printf("\n");
	    }
	}
    } else if (streq(action, "Substitute") ||
	    streq(action, "substitute")) {
	// Apply standard %-substitutions to the input string.
	int i;

	for (i = 0; i < argc; i++) {
	    CCS subs = NULL;

	    (void)util_substitute_params(argv[i], &subs);
	    printf("%s\n", subs);
	    putil_free(subs);
	}
    } else if (streq(action, "hash-object")) {
	int write_flag = 0;
	CCS dcode = NULL;
	ps_o ps;

	for (bsd_getopt_reset(); argv && *argv; ) {
	    int c;

	    // *INDENT-OFF*
	    static CS short_opts = "+s:w";
	    static struct option long_opts[] = {
		{"sha1",		required_argument, NULL, 's'},
		{"write",		no_argument,	   NULL, 'w'},
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
    } else if (streq(action, "Stat") || streq(action, "stat")) {
	// print vital statistics for specified files.
	int long_flag = 0, short_flag = 0, deref_flag = 0;
	CCS path;

	for (bsd_getopt_reset(); argv && *argv; ) {
	    int c;

	    // *INDENT-OFF*
	    static CS short_opts = "+alsL";
	    static struct option long_opts[] = {
		{"absolute-paths",	no_argument,	   NULL, 'a'},
		{"long",		no_argument,	   NULL, 'l'},
		{"short",		no_argument,	   NULL, 's'},
		{"dereference",	no_argument,	   NULL, 'L'},
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

	    ps = ps_newFromPath(*argv);
	    path = ps_get_abs(ps);

	    if (deref_flag) {
		statrc = stat64(path, &stbuf);
	    } else {
		statrc = lstat64(path, &stbuf);
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
		putil_warn("%s: not a regular file", path);
		ps_destroy(ps);
		continue;
	    }

	    if (ps_stat(ps, 1)) {
		rc = 1;
	    } else {
		size_t len;
		CS buf;

		len = putil_path_max() + 256 + 1;
		buf = putil_malloc(len);
		ps_format_user(ps, long_flag, short_flag, buf, len);
		fputs(buf, stdout);
		putil_free(buf);
	    }

	    ps_destroy(ps);
	}
    } else if (streq(action, "about")) {
	int short_flag = 0;

	for (bsd_getopt_reset(); argv && *argv; ) {
	    int c;

	    // *INDENT-OFF*
	    static CS short_opts = "+s";
	    static struct option long_opts[] = {
		{"short",		no_argument,	   NULL, 's'},
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
    } else if (argv && *argv && (streq(*argv, "-h") || streq(*argv, "--help"))) {
	// Always go to the server for help unless it's a pure client-side
	// action. That's why this is midway down - pure client actions
	// are above, anything requiring server interaction is below.
	rc = http_action(action, argv, 0);
    } else if (streq(action, "label")) {
	CS const *targv;

	for (bsd_getopt_reset(); argv && *argv; ) {
	    int c;

	    // *INDENT-OFF*
	    static CS short_opts = "+sli:p:";
	    static struct option long_opts[] = {
		{"long",		no_argument,	   NULL, 'l'},
		{"short",		no_argument,	   NULL, 's'},
		{"id",		required_argument, NULL, 'i'},
		{"project-name",	required_argument, NULL, 'p'},
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
    } else if (!strncmp(action, "namestate", 4)) {
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
    } else if (!strcmp(action, "Admin")) {
	if (argv[0] && streq(argv[0], "restart")) {
	    rc = http_restart();
	} else {
	    rc = http_action(action, argv, 0);
	}
    } else {
	if (streq(action, "help") && !argv[0]) {
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

	if (!strncmp(action, "lsb", 3)) {
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
_make_clean(int argc, CS const *argv)
{
    int i = 0;
    char clean[1024];
    char *pathMF = NULL;
#if defined(_WIN32)
    char *mkprog = "nmake /nologo";
#else	/*_WIN32*/
    char *mkprog = "make";
#endif	/*_WIN32*/

    for (i = 0; i < argc - 1; i++) {
	if (!strcmp("-f", argv[i])) {
	    pathMF = argv[i + 1];
	    break;
#if defined(_WIN32)
	} else if (!strcmp("/f", argv[i])) {
	    pathMF = argv[i + 1];
	    break;
#endif	/*_WIN32*/
	}
    }

    if (pathMF) {
	snprintf(clean, sizeof(clean) - 1, "%s -s -f %s clean", mkprog, pathMF);
    } else if (access("Makefile", R_OK) &&
	    access("makefile", R_OK) &&
#if !defined(_WIN32)
	    access("GNUmakefile", R_OK) &&
#endif	/*_WIN32*/
	    !access("build.xml", R_OK)) {
	snprintf(clean, sizeof(clean) - 1, "ant -q clean");
    } else {
	snprintf(clean, sizeof(clean) - 1, "%s -s clean", mkprog);
    }

    strcat(clean, " >");
    strcat(clean, DEVNULL);
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
    CS script = NULL;
    int proplevel = -1;
    int no_server = 0;
    int make_clean = 0;
    int rc = 0;

    // Figure out the full path to the current executable program.
    // Should be done asap, before anything happens to argv.
    if (!(exe = putil_getexecpath())) {
	putil_die("unable to determine path to argv[0]\n");
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
    prop_init(APPLICATION_NAME);
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

    // Default this to on; make auditing is tough without it.
    prop_override_true(P_MAKE_ONESHELL);

    // Parse the command line up to the first unrecognized item.
    // E.g. given "command -flag1 -flag2 arg1 -flag3 -flag4" we parse
    // only -flag1 and -flag2.
    for (bsd_getopt_reset();;) {
	int c;

	// *INDENT-OFF*
	static CS short_opts =
	    "+1acdhl:mo:p:qrs:tuv::wxAC:DEF:GH::I:LM:O:PQRSTUV:W:XY";
	static struct option long_opts[] = {
	    {"oneshell",	no_argument,	   NULL, '1'},
	    {"absolute-paths",	no_argument,	   NULL, 'a'},
	    {"agg-level",	required_argument, NULL, LF('A','G')},
	    {"audit-only",	no_argument,	   NULL, 'A'},
	    {"make-clean",	no_argument,	   NULL, 'c'},
	    {"directory",	required_argument, NULL, 'C'},
	    {"download-only",	no_argument,	   NULL, 'd'},
	    {"download-silent",	no_argument,	   NULL, 'D'},
	    {"dtrace",		required_argument, NULL, LF('D','T')},
	    {"client-platform",	required_argument, NULL, LF('C','P')},
	    {"error-strict",	no_argument,	   NULL, 'E'},
	    {"make-file",	required_argument, NULL, 'F'},
	    {"git",		no_argument,	   NULL, 'G'},
	    {"help",		no_argument,	   NULL, 'h'},
	    {"Help",		optional_argument, NULL, 'H'},
	    {"properties",	optional_argument, NULL, LF('P','*')},
	    {"identity-hash",	required_argument, NULL, 'I'},
	    {"log-file",	required_argument, NULL, 'l'},
	    {"log-file-temp",	no_argument,	   NULL, 'L'},
	    {"make-depends",	optional_argument, NULL, 'M'},
	    {"mem-debug",	optional_argument, NULL, LF('D','M')},
	    {"members-only",	no_argument,	   NULL, 'm'},
	    {"output-file",	required_argument, NULL, 'o'},
	    {"Output-file",	required_argument, NULL, 'O'},
	    {"project-name",	required_argument, NULL, 'p'},
	    {"pager",		no_argument,	   NULL, 'P'},
	    {"profile",		no_argument,	   NULL, LF('P','%')},
	    {"quiet",		no_argument,	   NULL, 'q'},
	    {"extra-quiet",	no_argument,	   NULL, 'Q'},
	    {"leave-roadmap",	no_argument,	   NULL, 'r'},
	    {"reuse-roadmap",	no_argument,       NULL, 'R'},
	    {"restart",		no_argument,	   NULL, LF('R','L')},
	    {"server",		required_argument, NULL, 's'},
	    {"strict",		no_argument,	   NULL, 'S'},
	    {"script",		required_argument, NULL, LF('s','c')},
	    {"print-elapsed",	no_argument,	   NULL, 't'},
	    {"print-elapsed-x",	no_argument,	   NULL, 'T'}, // TODO compatibility,remove
	    {"upload-only",	no_argument,	   NULL, 'u'},
	    {"uncompressed-transfers",no_argument, NULL, 'U'},
	    {"verbosity",	optional_argument, NULL, 'v'},
	    {"local-verbosity",	required_argument, NULL, 'V'},
	    {"version",		optional_argument, NULL, LF('v','n')},
	    {"why",		no_argument,	   NULL, 'w'},
	    {"WFlag",		required_argument, NULL, 'W'},
	    {"exec-verbosity",	required_argument, NULL, 'x'},
	    {"execute-only",	no_argument,	   NULL, 'X'},
	    {"synchronous-transfers", no_argument,	   NULL, 'Y'},
	    {0,				0,		   NULL,  0 },
	};
	// *INDENT-ON*

	c = bsd_getopt(argc, argv, short_opts, long_opts, NULL);
	if (c == -1) {
	    break;
	}

	switch (c) {

	    case '1':
		prop_unset(P_MAKE_ONESHELL, 0);
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

	    // Undocumented hack - allows an automatic "make clean" which
	    // is not audited or counted in elapsed time.
	    case 'c':
		make_clean = 1;
		break;

	    case 'C':
		{
		    CCS cd_to;

		    (void)util_substitute_params(bsd_optarg, &cd_to);
		    if (!QuietMode) {
			fprintf(stderr, "+ cd %s\n", cd_to);
		    }
		    if (chdir(cd_to)) {
			putil_syserr(2, cd_to);
		    }
		    putil_free(cd_to);
		}
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
		    char buf[1024];

		    snprintf(buf, 1024, "%s.%lu.log",
			       prop_get_str(P_APP), (unsigned long)getpid());
		    prop_override_str(P_LOG_FILE, buf);
		    prop_override_ulong(P_LOG_FILE_TEMP, 1);
		}
		break;

	    case 'l':
		{
		    CS lbuf;

		    if ((lbuf = putil_realpath(bsd_optarg, 1))) {
			prop_override_str(P_LOG_FILE, lbuf);
			putil_free(lbuf);
		    } else {
			putil_syserr(0, bsd_optarg);
			prop_override_str(P_LOG_FILE, bsd_optarg);
		    }
		}
		break;

	    case 'F':
		prop_override_str(P_MAKE_FILE, bsd_optarg);
		break;

	    case 'G':
		prop_override_true(P_GIT);
		no_server = 1;
		break;

	    case 'M':
		if (!bsd_optarg || !strcmp(bsd_optarg, "D")) {
		    prop_override_str(P_MAKE_DEPENDS, "d");
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
		if (!bsd_optarg || !strcmp(bsd_optarg, "@")) {
		    prop_override_str(P_OUTPUT_FILE, DEVNULL);
		} else {
		    prop_override_str(P_OUTPUT_FILE, bsd_optarg);
		}
		break;

	    case 'p':
		prop_override_str(P_PROJECT_NAME, bsd_optarg);
		break;

	    case 'P':
		if (!(pager = getenv("PAGER"))) {
		    pager = "less";
		}
		break;

		// Generate runtime linker profile output. Works on
		// Solaris and Linux at least.
	    case LF('P', '%'):
#if !defined(_WIN32)
		putil_putenv("LD_PROFILE=" AUDITOR);
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

	    case LF('s', 'c'):
		script = bsd_optarg;
		break;

	    case 'T':
	    case 't':
		prop_unset(P_PRINT_ELAPSED, 1);
		prop_put_str(P_PRINT_ELAPSED, "-1");
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
		putil_die("no malloc debugger implemented");
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

    if (make_clean) {
	_make_clean(argc, argv);
    }

    // The command we've been asked to invoke.
    if ((action = argv[0])) {
	// Hack for typing ease: "make" is a synonym for "run make".
	// Similarly, "ant" is a synonym for "run ant".
	// And any word with non-alphanumeric chars must be a program to run.
	if (strpbrk(action, "/\\=+-")
	    || strstr(action, "make")
	    || !util_pathcmp(action, "sh")
	    || !util_pathcmp(action, "vcbuild")		// Windows only
	    || !util_pathcmp(action, "msbuild")		// Windows only
	    || !util_pathcmp(action, "ant")) {
	    action = "run";
	} else {
	    argv++;
	    argc--;
	}
    } else {
	// The default command ...
	action = "help";
    }

    // Default project name is the name of the dir containing the project
    // config file, with any numerical extension following a '-' removed.
    if (!prop_has_value(P_PROJECT_NAME)) {
	CCS pbase;
	CS pjname, ptr, dash;

	pbase = prop_get_str(P_BASE_DIR);
	if (pbase && (pjname = putil_basename(pbase)) && *pjname) {
	    ptr = putil_strdup(pjname);
	    if ((dash = strchr(ptr, '-')) && ISDIGIT(dash[1])) {
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
	(streq(action, "man") && (pager = prop_get_str(P_DOC_PAGER)))) {
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
	    putil_syserr(2, "pipe");
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
	    putil_syserr(2, "fork");
	}
    }
#endif	/*_WIN32*/

    // For debugging convenience allow server=nnnn to mean "localhost:nnnn"
    // and server=foo to mean "foo:8080" since those are the most common.
    if (prop_has_value(P_SERVER)) {
	const char *svr;

	svr = prop_get_str(P_SERVER);
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

    if (streq(action, "run")) {
	CCS cwd = NULL;
	CCS logfile = NULL;
	CCS rmap;
	CCS logprop;

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
	if ((cwd = util_get_cwd())) {
	    if (util_is_tmp(cwd) && !prop_is_true(P_EXECUTE_ONLY)) {
		putil_die("illegal tmp working directory: %s", cwd);
	    }
	} else {
	    putil_syserr(2, "util_get_cwd()");
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
	while (strchr(*argv, '=')) {
	    putil_putenv(*argv++);
	}

	// Determine whether the user requested a log file and
	// apply the %u, %p, etc. formats to it if so.
	if ((logprop = prop_get_str(P_LOG_FILE))) {
	    (void)util_substitute_params(logprop, &logfile);
	    // In case it's present but unwriteable.
	    if (logfile)
		(void)unlink(logfile);
	}

	// Work out the name of the roadmap file and store the result.
	if ((rmap = prop_get_str(P_ROADMAPFILE))) {
	    if (!putil_is_absolute(rmap)) {
		if ((rmap = putil_realpath(rmap, 1))) {
		    prop_override_str(P_ROADMAPFILE, rmap);
		    putil_free(rmap);
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
		putil_die("can't get a session at %s",
			  prop_get_str(P_SERVER));
	    }
	}

#if defined(_WIN32)
	/* TODO */
#else
	if (script) {
	    FILE *fp;

	    if (!(fp = fopen(script, "w"))) {
		putil_syserr(2, script);
	    } else {
		extern char **environ;		// Win32 declares this in stdlib.h
		size_t plen;
		char **pblock;
		char **envp;
		struct utsname sysdata;

		// This is just to get a sorted environment list.
		plen = prop_new_env_block_sizeA(environ);
		pblock = (char **)alloca(plen);
		memset(pblock, 0, plen);
		(void)prop_custom_envA(pblock, environ);

		fprintf(fp, "#!/bin/sh\n\n");

		if (!putil_uname(&sysdata)) {
		    fprintf(fp, "# Original host: %s %s %s %s %s\n\n",
			sysdata.sysname, sysdata.nodename, sysdata.release,
			sysdata.version, sysdata.machine);
		}

		fprintf(fp, "# Original environment settings:\n");
		for (envp = pblock + 1; *envp; envp++) {
		    char *t;

		    if (**envp == '_')
			continue;

		    fputs(": export ", fp);
		    for (t = *envp; *t && *t != '='; t++) {
			fputc(*t, fp);
		    }
		    fputc(*t++, fp);
		    fprintf(fp, "'%s'\n", t);
		}
		fprintf(fp, "\nset -x\n");
		fprintf(fp, "cd '%s' || exit 2\n", cwd);
		fprintf(fp, "exec %s\n", util_requote_argv(argv));

		(void)fchmod(fileno(fp), 0755);
		(void)fclose(fp);
		vb_printf(VB_STD, "rebuild script written to '%s'", script);
	    }
	}
#endif

	// For systems supporting DTrace: run the cmd with the specified
	// dtrace script.
	if (dscript) {
	    CS *dargv;

	    dargv = putil_malloc(sizeof(*dargv) * 6);
	    dargv[0] = "dtrace";
	    dargv[1] = "-s";
	    dargv[2] = dscript;
	    dargv[3] = "-c";
	    dargv[4] = util_requote_argv(argv);
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

	putil_free(logfile);

	if (prop_is_true(P_GIT)) {
	    git_fini();
	}

	make_fini();

	// If using a temp logfile, remove it now.
	if (prop_is_true(P_LOG_FILE_TEMP)) {
	    unlink(prop_get_str(P_LOG_FILE));
	}

	putil_free(cwd);
    } else if (streq(action, "roadmap")) {
	// Useful while testing roadmaps. May be dispensed with later.
	prop_override_str(P_ROADMAPFILE, ROADMAP_DEFAULT_NAME);
	prop_override_true(P_LEAVE_ROADMAP);
	mon_get_roadmap();
    } else if (streq(action, "shop")) {
	int cflag = 0;
	int gflag = 0;
	ca_o ca;
	CCS rwd;

	// This special action is useful for internal tests of shopping
	// capabilities without changing server state, using a saved
	// roadmap file.

	while (*argv && **argv == '-') {
	    if (!strcmp(*argv, "-C")) {
		// Work with a predetermined cmd index rather than a cmd line.
		cflag = 1;
	    } else if (!strcmp(*argv, "-G")) {
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
	ca_set_rwd(ca, rwd = util_get_rwd());
	putil_free(rwd);
	ca_set_started(ca, 1);

	// We almost always want these while debugging...
	vb_addbit(VB_SHOP);
	vb_addbit(VB_WHY);

	shop_init();
	if (cflag) {
	    rc = shop(ca, argv[0], gflag);
	} else {
	    CCS cmdline;

	    cmdline = util_requote_argv(argv);
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
