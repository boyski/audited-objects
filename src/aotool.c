// Copyright (c) 2002-2025 David Boyce.  All rights reserved.

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
///
/// Handles parsing of command-line options, certain
/// client-side-only actions, and general initialization
/// of the client program.

#include "AO.h"

#include "CA.h"
#include "CODE.h"
#include "HEAP.h"
#include "MAKE.h"
#include "MON.h"
#include "PA.h"
#include "PN.h"
#include "PS.h"
#include "PREFS.h"
#include "PROP.h"
#include "RE.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

#include "bsd_getopt.h"

#if !defined(BSD)
#include <alloca.h>
#endif  // BSD

static char *ProgramName = "ao";  // could derive dynamically?

static char self_license[] = "\
Copyright (c) 2002-2025 David Boyce.  All rights reserved.\n\
Warning: this computer software is protected by copyright law and\n\
international treaties. Unauthorized reproduction or distribution of\n\
this software, or any portion of it, may result in severe civil and\n\
criminal penalties, and will be prosecuted to the maximum extent\n\
possible under the law.\n\
\n\
This program is free software: you can redistribute it and/or modify\n\
it under the terms of the GNU Affero General Public License as\n\
published by the Free Software Foundation, either version 3 of the\n\
License, or (at your option) any later version.\n\
\n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU Affero General Public License for more details.\n\
\n\
You should have received a copy of the GNU Affero General Public License\n\
along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\
";

static char bsd_license[] = "\
Copyright (c) 1990, 2008\n\
    The Regents of the University of California.  All rights reserved.\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions\n\
are met:\n\
1. Redistributions of source code must retain the above copyright\n\
notice, this list of conditions and the following disclaimer.\n\
2. Redistributions in binary form must reproduce the above copyright\n\
notice, this list of conditions and the following disclaimer in the\n\
documentation and/or other materials provided with the distribution.\n\
3. All advertising materials mentioning features or use of this software\n\
must display the following acknowledgement:\n\
    This product includes software developed by the University of\n\
    California, Berkeley and its contributors.\n\
4. Neither the name of the University nor the names of its contributors\n\
may be used to endorse or promote products derived from this software\n\
without specific prior written permission.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND\n\
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n\
ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE\n\
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n\
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n\
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n\
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n\
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n\
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n\
SUCH DAMAGE.\n\
";

static char cjson_license[] = "\
Copyright (c) 2009-2017 Dave Gamble and cJSON contributors\n\
\n\
Permission is hereby granted, free of charge, to any person obtaining a copy\n\
of this software and associated documentation files (the \"Software\"), to deal\n\
in the Software without restriction, including without limitation the rights\n\
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
copies of the Software, and to permit persons to whom the Software is\n\
furnished to do so, subject to the following conditions:\n\
\n\
The above copyright notice and this permission notice shall be included in\n\
all copies or substantial portions of the Software.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n\
THE SOFTWARE.\n\
";

static char pcre2_license[] = "\
           Copyright (c) 2016-2024 University of Cambridge\n\
\n\
-----------------------------------------------------------------------------\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions are met:\n\
\n\
    * Redistributions of source code must retain the above copyright notice,\n\
      this list of conditions and the following disclaimer.\n\
\n\
    * Redistributions in binary form must reproduce the above copyright\n\
      notice, this list of conditions and the following disclaimer in the\n\
      documentation and/or other materials provided with the distribution.\n\
\n\
    * Neither the name of the University of Cambridge nor the names of its\n\
      contributors may be used to endorse or promote products derived from\n\
      this software without specific prior written permission.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n\
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n\
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE\n\
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\n\
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF\n\
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS\n\
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN\n\
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n\
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE\n\
POSSIBILITY OF SUCH DAMAGE.\n\
";

static char xxhash_license[] = "\
Copyright (C) 2012-2023 Yann Collet\n\
\n\
BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions are\n\
met:\n\
\n\
   * Redistributions of source code must retain the above copyright\n\
     notice, this list of conditions and the following disclaimer.\n\
   * Redistributions in binary form must reproduce the above\n\
     copyright notice, this list of conditions and the following disclaimer\n\
     in the documentation and/or other materials provided with the\n\
     distribution.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n\
\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n\
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n\
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n\
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n\
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n\
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n\
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n\
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\
\n\
You can contact the author at:\n\
  - xxHash homepage: https://www.xxhash.com\n\
  - xxHash source repository: https://github.com/Cyan4973/xxHash\n\
";

static char banner[] =
"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@";

void print_license(const char *, const char *);

void
print_license(const char *title, const char *text)
{
    fputs(banner, stdout);
    fputs("\n\t\t\t\t", stdout);
    fputs(title, stdout);
    fputs("\n", stdout);
    fputs(banner, stdout);
    fputs("\n\n", stdout);
    fputs(text, stdout);
    fputs("\n\n", stdout);
}

static void
about_client(void)
{
    print_license("BSD", bsd_license);
    print_license("cJSON", cjson_license);
    print_license("PCRE2", pcre2_license);
    print_license("XXHASH", xxhash_license);
}

/// @cond static
#define ROADMAP_DEFAULT_NAME            "roadmap.cdb"

#define HELP_FMT "%-12s - %s\n"
#define USAGE_FMT "   %-15s %s\n"
/// @endcond static

/// Macro for making unique long flags with getopt_long(). Some long
/// flags do not have a corresponding single-char name. We create an
/// artificial name for these by combining the first letter of the
/// long flag with an arbitrary second char. This provides a
/// namespace allowing up to 256 different long flags for each
/// leading letter.
/// Note that the option.val member which holds this is really an int.
#define LF(first, secondary)    (((first)<<8) + (secondary))

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
_usg_line(FILE *fp, CCS flag, CCS prop, CCS desc)
{
    if (!prop_is_public(prop)) {
        return;
    }

    if (desc) {
        fprintf(fp, USAGE_FMT, flag, desc);
    } else {
        char buf[1024];

        snprintf(buf, charlen(buf), "%s [=%s]", prop_desc(prop), prop);
        fprintf(fp, USAGE_FMT, flag, buf);
    }
}

// Internal service routine.
static void
_usage(int rc)
{
    FILE *fp;

    fp = rc ? stderr : stdout;

    fprintf(fp, "USAGE: %s [<options>]", ProgramName);
    fprintf(fp, " { make | run <prog> | <action> } [args...]\n");
    fprintf(fp, "FLAGS:\n");

    fprintf(fp, USAGE_FMT, "-h", "Print this usage summary");
    fprintf(fp, USAGE_FMT, "-H", "Print current properties");
    _usg_line(fp, "-a", P_ABSOLUTE_PATHS, NULL);
    fprintf(fp, USAGE_FMT, "-C dir", "Change to dir before doing anything");
    _usg_line(fp, "-d", P_DOWNLOAD_ONLY, NULL);
    _usg_line(fp, "-F file", P_MAKE_FILE, NULL);
    _usg_line(fp, "-l file", P_LOG_FILE, NULL);
    _usg_line(fp, "-MD", P_MAKE_DEPENDS, NULL);
    _usg_line(fp, "-m", P_MEMBERS_ONLY, NULL);
    _usg_line(fp, "-o file", P_OUTPUT_FILE, NULL);
    _usg_line(fp, "-p name", P_PROJECT_NAME, NULL);
    fprintf(fp, USAGE_FMT, "-q", "Quiet mode: suppress verbosity");
    _usg_line(fp, "-t", P_PRINT_ELAPSED, NULL);
    _usg_line(fp, "-u", P_UPLOAD_ONLY, NULL);
    fprintf(fp, USAGE_FMT, "-vXX,YY,ZZ", "Set verbosity flags (use -v? to see choices");
    fprintf(fp, USAGE_FMT, "--version", "Print version and exit");
    fprintf(fp, USAGE_FMT, "-w", "Explain why commands cannot be recycled");
    fprintf(fp, USAGE_FMT, "-Wm,flag,value", "Pass -flag=value to makefile generator");
    fprintf(fp, USAGE_FMT, "-x", "Print each command line as executed");
    _usg_line(fp, "-X", P_EXECUTE_ONLY, NULL);

    fprintf(fp, "EXAMPLES:\n");
    fprintf(fp, "   %s help\n", ProgramName);
    fprintf(fp, "   %s ping\n", ProgramName);
    fprintf(fp, "   %s -o@ -F Makefile.new make -s clean all\n", ProgramName);
    fprintf(fp, "   %s make clean all\n", ProgramName);
    fprintf(fp, "   %s lsbuilds -s\n", ProgramName);

    putil_exit(rc);
}

/// Print the version of this application to stdout.
/// @param[in] full      if true, print versions of all 3rd-party libs too
static void
_print_version(int full)
{
    printf("%s=%s\n", "AO", prop_get_str(P_VERSION));

    if (full) {
        fputs(putil_builton(), stdout);
#if defined(_DEBUG)
        fputs(" (DEBUG)", stdout);
#endif  // _DEBUG
        fputs("\n", stdout);
        printf("pcre2=%s\n", re_version__());
        printf("kazlib=1.20\n"); // Abandoned, will never update
    }
}

/// Internal service routine. Handles sub-commands.
static int
_action(CCS action, int argc, CS const *argv)
{
    int rc = 0;

    if (!strcmp(action, "Property") || !strcmp(action, "property")) {
        // Print the current value of the named property.
        for (int i = 0; i < argc; i++) {
            CCS prop = argv[i];
            if (vb_bitmatch(VB_STD)) {
                printf("%s=%s", prop, prop_get_str(prop));
            }
        }
    } else if (!strcmp(action, "Substitute") || !strcmp(action, "substitute")) {
        // Apply standard %-substitutions to the input string.
        int i;

        for (i = 0; i < argc; i++) {
            CCS subs = NULL;

            (void)util_substitute_params(argv[i], &subs);
            printf("%s\n", subs);
            putil_free(subs);
        }
    } else if (!strcmp(action, "hash")) {
        CCS dcode = NULL;
        ps_o ps;

        for (bsd_getopt_reset(); argv && *argv; ) {
            int c;

            // *INDENT-OFF*
            static CS short_opts = "+s:";
            static struct option long_opts[] = {
                {"sha1",                required_argument, NULL, 's'},
                {0,                     0,                 NULL,  0 },
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
                default:
                    break;
            }
        }
        argv += (bsd_optind - 1);

        ps = ps_newFromPath(*argv);
        if (dcode) {
            ps_set_dcode(ps, dcode);
        } else if (ps_stat(ps, 1)) {
            putil_syserr(2, *argv);
        }

        printf("%s\n", ps_get_dcode(ps));

        ps_destroy(ps);
        putil_free(dcode);
    } else if (!strcmp(action, "Stat") || !strcmp(action, "stat")) {
        // print vital statistics for specified files.
        int long_flag = 0, short_flag = 0, deref_flag = 0;
        CCS path;

        for (bsd_getopt_reset(); argv && *argv; ) {
            int c;

            // *INDENT-OFF*
            static CS short_opts = "+alsL";
            static struct option long_opts[] = {
                {"absolute-paths",      no_argument,       NULL, 'a'},
                {"long",                no_argument,       NULL, 'l'},
                {"short",               no_argument,       NULL, 's'},
                {"dereference",         no_argument,       NULL, 'L'},
                {0,                     0,                 NULL,  0 },
            };
            // *INDENT-ON*

            c = bsd_getopt(argc + 1, argv - 1, short_opts, long_opts, NULL);
            if (c == -1 || c == '?') {
                break;
            }

            switch (c) {
                case 'a':
                    prop_set_bool(P_ABSOLUTE_PATHS, true);
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
            } else if (!S_ISREG(stbuf.st_mode) && !S_ISLNK(stbuf.st_mode)) {
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
                (void)ps_format_user(ps, long_flag, short_flag, buf, len);
                fputs(buf, stdout);
                putil_free(buf);
            }

            ps_destroy(ps);
        }
    } else if (!strcmp(action, "about")) {
        int short_flag = 0;

        for (bsd_getopt_reset(); argv && *argv; ) {
            int c;

            // *INDENT-OFF*
            static CS short_opts = "+s";
            static struct option long_opts[] = {
                {"short",               no_argument,       NULL, 's'},
                {0,                     0,                 NULL,  0 },
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
            (void)print_license("Audited Objects", self_license);

            fputs("This software incorporates all or parts ", stdout);
            fputs("of the following, whose\ncontributions ", stdout);
            fputs("are gratefully acknowledged:\n\n", stdout);
            about_client();
        }
        _print_version(0);
    } else if (!strcmp(action, "help")) {
        // This documents actions which are PURELY client-side
        // aside from the "--" variants.
        // If they have any server part, e.g. "label" or "ADMIN",
        // then let the server document them.
        printf(HELP_FMT, "--", "run and audit the specified command");
        printf(HELP_FMT, "hash", "print the AO hash for each named file");
        printf(HELP_FMT, "property", "print the value of the named property");
        printf(HELP_FMT, "stat", "print vital statistics for specified files");
        printf(HELP_FMT, "substitute", "apply standard %-formatting to the input string");
        printf("\n");
    } else {
        fprintf(stderr, "%s: Error: unrecognized action '%s'\n", ProgramName, action);
    }

    return rc;
}

/// Search upward from a given directory toward the filesystem
/// root looking for the named file.
/// @param[in] name     filename or trailing path fragment to search for
/// @param[in] dir      starting directory (cwd if NULL)
/// @return path to found file on the heap, or NULL
static CS
_find_file_up(CCS ending, CS dir)
{
    CS result;

    if (dir) {
        dir = putil_realpath(dir, 1);
    } else {
        if (!(dir = (CS)util_get_cwd())) {
            putil_syserr(0, "util_get_cwd");
            exit(2);
        }
    }

    while (strcmp(dir, "/")) {
        CS pdir;

        TRY(asprintf(&result, "%s%s%s", dir, DIRSEP(), ending), "asprintf()");
        if (!access(result, F_OK)) {
            putil_free(dir);
            return result;
        } else {
            putil_free(result);
        }

        // Quit when we reach the root even if the root is not
        // called '/' (like on Windows).
        if ((pdir = putil_dirname(dir))) {
            if (strlen(pdir) >= strlen(dir)) {
                putil_free(pdir);
                break;
            } else {
                putil_free(dir);
                dir = pdir;
            }
        }
    }

    putil_free(dir);
    return NULL;
}

/// Entry point for the client program.
/// @param[in] argc     the number of arguments
/// @param[in] argv     the classic argument vector taken by every main routine
/// @return the exit code of the process
int
main(int argc, CS *argv)
{
    CCS action;
    CS dscript = NULL;
    int proplevel = -1;
    int rc = 0;

    heap_init();

    // Initialize verbosity. Should be done before most
    // other initializations since they may use verbosity.
    vb_init();

    // Initialize the properties database and at the same time
    // arrange for it to be finalized at exit. Doing it this
    // way ensures that it will be finalized last, meaning that
    // other exit and error handling routines will have access
    // to properties until the bitter end. Really, the only reason
    // to explicitly free data right before exit like this is
    // to avoid spurious warnings from memory leak detectors like
    // valgrind and make real problems more obvious.
    CCS cfgfile = _find_file_up(".ao/ao_config.json", NULL);
    prop_init(cfgfile);
    putil_free(cfgfile);

    // Initialize the hash-code generator.
    code_init();

    // Ugly hack. There may be a better way to handle this within the getopt
    // API but for now this fixes the UI.
    for (int i = 0; argv[i]; i++) {
        if (!strcmp(argv[i], "--")) {
            argv[i] = "run";
        }
    }

    // Parse the command line up to the first unrecognized item.
    // E.g. given "command -flag1 -flag2 arg1 -flag3 -flag4" we parse
    // only -flag1 and -flag2.
    for (bsd_getopt_reset();;) {
        int c;

        // *INDENT-OFF*
        static CS short_opts = "+adf:hl:mo:p:qrtuv::wxAC:EH::M:O:RSTUV:W:XY";
        static struct option long_opts[] = {
            {"absolute-paths",  no_argument,       NULL, 'a'},
            {"agg-level",       required_argument, NULL, LF('A', 'G')},
            {"audit-only",      no_argument,       NULL, 'A'},
            {"directory",       required_argument, NULL, 'C'},
            {"download-only",   no_argument,       NULL, 'd'},
            {"dtrace",          required_argument, NULL, LF('D', 'T')},
            {"client-platform", required_argument, NULL, LF('C', 'P')},
            {"error-strict",    no_argument,       NULL, 'E'},
            {"make-file",       required_argument, NULL, 'f'},
            {"help",            no_argument,       NULL, 'h'},
            {"Help",            optional_argument, NULL, 'H'},
            {"properties",      optional_argument, NULL, LF('P', '*')},
            {"log-file",        required_argument, NULL, 'l'},
            {"log-time-stamp",  no_argument,       NULL, LF('L', 'S')},
            {"make-depends",    optional_argument, NULL, 'M'},
            {"mem-debug",       optional_argument, NULL, LF('D', 'M')},
            {"members-only",    no_argument,       NULL, 'm'},
            {"output-file",     required_argument, NULL, 'o'},
            {"Output-file",     required_argument, NULL, 'O'},
            {"project-name",    required_argument, NULL, 'p'},
            {"profile",         no_argument,       NULL, LF('P', '%')},
            {"quiet",           no_argument,       NULL, 'q'},
            {"leave-roadmap",   no_argument,       NULL, 'r'},
            {"reuse-roadmap",   no_argument,       NULL, 'R'},
            {"strict",          no_argument,       NULL, 'S'},
            {"print-elapsed",   no_argument,       NULL, 't'},
            {"upload-only",     no_argument,       NULL, 'u'},
            {"uncompressed-transfers", no_argument, NULL, 'U'},
            {"verbosity",       optional_argument, NULL, 'v'},
            {"local-verbosity", required_argument, NULL, 'V'},
            {"version",         optional_argument, NULL, LF('v', 'n')},
            {"why",             no_argument,       NULL, 'w'},
            {"WFlag",           required_argument, NULL, 'W'},
            {"exec-verbosity",  required_argument, NULL, 'x'},
            {"execute-only",    no_argument,       NULL, 'X'},
            {"synchronous-transfers", no_argument,         NULL, 'Y'},
            {0,                         0,                 NULL,  0 },
        };
        // *INDENT-ON*

        c = bsd_getopt(argc, argv, short_opts, long_opts, NULL);
        if (c == -1) {
            break;
        }

        switch (c) {

            case 'a':
                prop_set_bool(P_ABSOLUTE_PATHS, true);
                break;

            case LF('A', 'G'):
                prop_set_str(P_AGGREGATION_STYLE, bsd_optarg);
                break;

            case 'A':
                prop_set_bool(P_AUDIT_ONLY, true);
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
                prop_set_str(P_MONITOR_PLATFORM, bsd_optarg);
                break;

            case 'd':
                prop_set_bool(P_DOWNLOAD_ONLY, true);
                break;

            case LF('D', 'T'):
                dscript = bsd_optarg;
                break;

            case 'E':
                prop_set_long(P_STRICT_ERROR, 1);
                break;

            case LF('L', 'S'):
                prop_set_ulong(P_LOG_TIME_STAMP, 1);
                break;

            case 'l':
                {
                    CS lbuf;

                    if (*bsd_optarg == ':') {
                        prop_set_ulong(P_LOG_TIME_STAMP, 1);
                        bsd_optarg++;
                    }

                    if (putil_is_absolute(bsd_optarg)) {
                        prop_set_str(P_LOG_FILE, bsd_optarg);
                    } else if ((lbuf = putil_realpath(bsd_optarg, 1))) {
                        prop_set_str(P_LOG_FILE, lbuf);
                        putil_free(lbuf);
                    } else {
                        putil_syserr(0, bsd_optarg);
                        prop_set_str(P_LOG_FILE, bsd_optarg);
                    }
                }
                break;

            case 'f':
                prop_set_str(P_MAKE_FILE, bsd_optarg);
                break;

            case 'M':
                if (!bsd_optarg || !strcmp(bsd_optarg, "D")) {
                    prop_set_str(P_MAKE_DEPENDS, ".d");
                } else {
                    prop_set_str(P_MAKE_DEPENDS, bsd_optarg);
                }
                break;

            case 'm':
                prop_set_bool(P_MEMBERS_ONLY, true);
                break;

            case 'o':
                // fallthrough
            case 'O':
                if (!bsd_optarg || !strcmp(bsd_optarg, "@")) {
                    prop_set_str(P_OUTPUT_FILE, DEVNULL);
                } else {
                    prop_set_str(P_OUTPUT_FILE, bsd_optarg);
                }
                break;

            case 'p':
                prop_set_str(P_PROJECT_NAME, bsd_optarg);
                break;

                // Generate runtime linker profile output.
            case LF('P', '%'):
                putil_putenv("LD_PROFILE=" AUDITOR);
                break;

            case 'q':
                QuietMode = 1;
                vb_addstr("-");
                break;

            case 'R':
                prop_set_bool(P_REUSE_ROADMAP, true);
                prop_set_ulong(P_DOWNLOAD_ONLY, 2);
                // fallthrough
            case 'r':
                prop_set_str(P_ROADMAPFILE, ROADMAP_DEFAULT_NAME);
                prop_set_bool(P_LEAVE_ROADMAP, true);
                break;

            case 'S':
                prop_set_ulong(P_STRICT, 1);
                prop_set_ulong(P_STRICT_DOWNLOAD, 1);
                prop_set_ulong(P_STRICT_UPLOAD, 1);
                break;

            case 't':
                prop_set_str(P_PRINT_ELAPSED, "-1");
                break;

            case 'u':
                prop_set_ulong(P_UPLOAD_ONLY, 1);
                break;

            case 'U':
                prop_set_ulong(P_UNCOMPRESSED_TRANSFERS, 1);
                break;

            case 'V':           // keep verbosity out of auditor
                // prop_unexport(P_VERBOSITY, 1);
                // fallthrough
            case 'v':
                vb_addstr(bsd_optarg);
                // With no arg, print known flags and exit.
                if (!bsd_optarg) {
                    exit(0);
                }
                break;

            case LF('D', 'M'):
#if defined(__APPLE__)
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
#else   // __APPLE__
                putil_die("no malloc debugger implemented");
#endif  // __APPLE__
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
                        prop_set_str(P_WFLAG, str);
                        putil_free(str);
                    } else {
                        prop_set_str(P_WFLAG, bsd_optarg);
                    }
                }
                break;

            case 'x':
                vb_addbit(VB_EXEC);
                break;

            case 'X':
                prop_set_bool(P_EXECUTE_ONLY, true);
                prop_set_ulong(P_PRINT_ELAPSED, 1);
                break;

            case 'Y':
                prop_set_bool(P_SYNCHRONOUS_TRANSFERS, true);
                break;

            case 'h':
                _usage(0);
                break;

            case LF('P', '*'):  // alias to --Help for mnemonic reasons
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
        argv++;
        argc--;
    } else {
        // The default command.
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
            prop_set_str(P_PROJECT_NAME, ptr);
            putil_free(ptr);
        }
    }

    // Recycling would break full-makefile generation.
    if (prop_has_value(P_MAKE_FILE) && !prop_has_value(P_MAKE_DEPENDS)) {
        prop_set_ulong(P_UPLOAD_ONLY, 1);
    }

    // Diagnostic: dump properties and quit.
    if (proplevel >= 0) {
        prop_help(proplevel, vb_bitmatch(VB_STD));
        exit(0);
    }

    // Arrange for the elapsed time to be printed out at exit time
    // if requested by user property. The property is not a boolean
    // but a mimimum number of seconds which must elapse in order
    // to trigger output.
    if ((ElapsedMinimum = prop_get_long(P_PRINT_ELAPSED))) {
        StartTime = time(NULL);
        atexit(_print_elapsed_time);
    }

    if (!strcmp(action, "run")) {
        CCS cwd = NULL;
        CCS logfile = NULL;
        CCS rmap;
        CCS logprop;

        if (!argv || !*argv) {
            _usage(1);
        }

        if (!(cwd = util_get_cwd())) {
            putil_syserr(0, "getcwd()");
            exit(2);
        }

        // The auditor ignores anything it considers to be a temp
        // file, which can cause terribly confusing behavior if
        // run on a project rooted in (say) /tmp. It's a natural
        // tendency for someone setting up a quick test case to
        // put it in /tmp and then spend hours trying to figure
        // out why it's behaving strangely. To help avoid this
        // we disallow the auditor from running when the CWD is
        // one we consider a repository for temp files.
        if (strlen(cwd) >= 4 && !util_pathcmp(endof(cwd) - 4, "/tmp")) {
            if (!prop_is_true(P_EXECUTE_ONLY)) {
                putil_die("illegal tmp working directory: %s", cwd);
            }
        }

        // Reserve a block of env space for the current cmd depth.
        prop_set_ulong(P_DEPTH, 0);

        // Reserve a block of env space for the parent cmd id.
        // Seed it with our pid.
        prop_set_ulong(P_PCMDID, getpid());

        // Reserve a block of env space for the parent cmd code.
        prop_set_str(P_PCCODE, CSV_NULL_FIELD);

        // This needs to be present in the env from the start even when off.
        // But being a boolean it only needs room for one digit.
        prop_set_ulong(P_AGGREGATED_SUBCMD, 0);

        // Allow the cmd to be preceded by EV's in the manner of the shell.
        while (strchr(*argv, '=')) {
            putil_putenv(*argv++);
        }

        // Determine whether the user requested a log file and
        // apply the %u, %p, etc. formats to it if so.
        if ((logprop = prop_get_str(P_LOG_FILE))) {
            (void)util_substitute_params(logprop, &logfile);
            // In case it's present but unwriteable.
            if (logfile && access(logfile, W_OK)) {
                (void)unlink(logfile);
            }
        }

        // Work out the name of the roadmap file and store the result.
        if ((rmap = prop_get_str(P_ROADMAPFILE))) {
            if (!putil_is_absolute(rmap)) {
                if ((rmap = putil_realpath(rmap, 1))) {
                    prop_set_str(P_ROADMAPFILE, rmap);
                    putil_free(rmap);
                } else {
                    putil_syserr(2, prop_get_str(P_ROADMAPFILE));
                }
            }
        } else if (prop_has_value(P_SERVER)) {
            rmap = tempnam(NULL, "rmap.");
            prop_set_str(P_ROADMAPFILE, rmap);
            putil_free(rmap);
        }

        if (prop_has_value(P_SERVER)) {
            // Get the new roadmap.
            if (!prop_is_true(P_REUSE_ROADMAP) && !prop_is_true(P_UPLOAD_ONLY)) {
                mon_get_roadmap();
            }

            // Start a session. We do this *after* getting the roadmap
            // to avoid session expiration during roadmap-getting.
            // That would only be a risk for a very slow roadmap or a
            // very fast session expiration but it could happen.
            if (mon_begin_session()) {
                putil_die("can't get a session at %s", prop_get_str(P_SERVER));
            }
        }

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

        make_init(ProgramName);

        // RUN AND AUDIT THE COMMAND.
        rc = run_cmd(ProgramName, (CS *)argv, logfile);

        putil_free(logfile);

        make_fini();

        putil_free(cwd);
    } else {
        rc = _action(action, argc, argv);
    }

    code_fini();

    prop_fini();

    vb_fini();

    heap_fini();

    return rc;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
