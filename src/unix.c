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
/// @brief Provide Unix-specific code to kick off the top-level
/// audited command and monitor it and its children, receiving
/// "audit packets", and processing them.

#include "AO.h"

#include "ACK.h"
#include "CA.h"
#include "FREE.h"
#include "MON.h"
#include "PROP.h"
#include "TRY.h"

#include <signal.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "Interposer/preload.h"

// This is a "self-pipe" - the monitor process uses it so we can
// handle "asynchronous-end conditions" - core dumps and the like -
// via the normal select loop. When the top-level build process
// ends for any reason, the read end of this pipe becomes ready
// and the monitor is alerted to that fact. Without this the
// monitor would hang until the select() timeout.
static int done_pipe[2];

static int doneflag = 0;
static int ExitStatus = 0;
static int Started = 0;

/****************************************************************************
 * The two funcs (interposer_preload_{on,off}) turn a preloaded library
 * on and off (duh) by adding to or subtracting from the LD_PRELOAD EV.
 * On Mac OSX the equivalent var is DYLD_INSERT_LIBRARIES which only works
 * in the presence of DYLD_FORCE_FLAT_NAMESPACE.
 ***************************************************************************/

static void
_interposer_die(const char *string)
{
    if (string) {
        fprintf(stderr, "INTERPOSER: Error: %s: %s\n", string, strerror(errno));
    } else {
        fprintf(stderr, "INTERPOSER: Error: %s\n", strerror(errno));
    }
    exit(2);
}

static void
_interposer_preload_on(const char *so, const char *base)
{
    char *ldp, *nldp;
    char lib_buf[PATH_MAX];

    (void)snprintf(lib_buf, sizeof(lib_buf), "%s/lib64/%s", base, so);

    ldp = getenv(PRELOAD_EV);

    // The documentation says LD_PRELOAD* is a *space* separated
    // list, but that's kind of unusual and in fact colons seem
    // to work fine so we use them. Spaces work fine too though.
    if (ldp && *ldp) {
        // Currently we put our .so after any existing libraries.
        // It remains to be seen whether before or after is a better plan.
        if (asprintf(&nldp, "%s=%s:%s", PRELOAD_EV, ldp, lib_buf) < 0) {
            _interposer_die(NULL);
        }
    } else {
        if (asprintf(&nldp, "%s=%s", PRELOAD_EV, lib_buf) < 0) {
            _interposer_die(NULL);
        }
    }

    if (putenv(nldp))
        _interposer_die("putenv");

#if defined(__APPLE__)
    if (putenv("DYLD_FORCE_FLAT_NAMESPACE=1"))
        _interposer_die("putenv");
#endif  /*__APPLE__*/
}

#if 0

static void
_interposer_preload_off(const char *so, char *const envp[], int all)
{
    extern char **environ;
    char **env;

    env = envp ? (char **)envp : environ;

    for (; *env; env++) {
        // Match any EV beginning with LD_PRELOAD=.
        if (!strncmp(*env, PRELOAD_EV "=", sizeof(PRELOAD_EV) + 1)) {
            char *ev, *ldpname, *nbuf, *lib, *ldpath, *e;
            size_t evlen;

            if (all) {
                **env = '_';
                continue;
            }

            ev = *env;
            evlen = strlen(ev);

            // Copy the EV into a buffer we can modify with impunity,
            // then snip it into a name/value pair.
            TRY(ldpname = malloc(evlen + 2), "malloc()");
            strcpy(ldpname, ev);
            if (!(ldpath = strchr(ldpname, '='))) {
                return; // can't happen but makes gcc -fanalyzer happier
            }

            // Start the new construction buffer with "<EV>=" ...
            TRY(nbuf = malloc(evlen + 2), "malloc()");
            strcpy(nbuf, ldpname);
            strcat(nbuf, "=");

            // Work our way through the space-separated list with strtok().
            // Since colon-separated lists also work they're handled too.
            // Paths ending with our shared-object name are skipped,
            // anything else is copied to the new buffer.
            // Pathnames containing spaces are SOL but blame that
            // on whoever decided LD_PRELOAD* was a space-separated list.
            for (lib = strtok(ldpath, " :"); lib; lib = strtok(NULL, " :")) {
                if (!endswith(lib, so)) {
                    strcat(nbuf, lib);
                    strcat(nbuf, " ");
                }
            }

            // Trim trailing space.
            e = strchr(nbuf, '\0');
            while (isspace((int)*--e))
                *e = '\0';

            // If operating on a custom environment, modify it in place.
            // If operating on the standard 'environ' array, interact
            // with it via standard OS APIs.
            // Ideally, esp. when dealing with
            // an explicit env block, we should shuffle other EVs
            // down and remove the LD_PRELOAD* slot entirely in
            // cases where it should be empty. But instead we end
            // up doing the equivalent of 'putenv("LD_PRELOAD=")'.
            // This is where we miss the BSD unsetenv() function.
            if (envp) {
                strcpy(ev, nbuf);
                FREE(nbuf);
            } else {
                putenv(nbuf);
            }

            FREE(ldpname);
        }
    }
}

// Potentially useful for debugging purposes but has no "production" value.
static void
_interposer_preload_dbg(const char *location)
{
    char *pld;

    if ((pld = getenv(PRELOAD_EV))) {
        fprintf(stderr, "%s: %s='%s'\n", location, PRELOAD_EV, pld);
    }
}
#endif // 0

// Used to alert the monitor that the top-level child has ended.
static void
_unix_sigchld(int signum)
{
    UNUSED(signum);
    doneflag = 1;
    TRY(write(done_pipe[1], DONE_TOKEN, strlen(DONE_TOKEN)) != -1, "write(done_pipe[1])");
    TRY(write(done_pipe[1], "\n", 1) != -1, "write(done_pipe[1])");
    close(done_pipe[1]);
}

// Used to dump debug data from a signal.
static volatile sig_atomic_t dumpflag;
static void
_unix_sigusr1(int signum)
{
    dumpflag = signum;
}

static void
_unix_sig_setup(void)
{
    struct sigaction sa = {0};

    // Alert the monitor loop when the child is no longer alive.
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = _unix_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_RESETHAND;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        putil_syserr(2, "sigaction(SIGCHLD)");
        exit(2);
    }

    // Debug feature: dump the current audit DB upon catching a signal.
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = _unix_sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        putil_syserr(2, "sigaction(SIGUSR1)");
        exit(2);
    }
}

static CS
_unix_read_available(int fd)
{
    CS buf;
    ssize_t bufsize = 1024, len, num;

    TRY(buf = malloc(bufsize), "malloc()");

    for (len = 0, buf[0] = '\0';;) {
        num = read(fd, buf + len, bufsize - len - 1);
        if (num < 0) {
            if (errno != EAGAIN) {
                putil_syserr(2, "read");
                exit(2);
            }
        } else if (num == 0) {
            if (len && buf[len - 1] != '\n') {
                putil_warn("Incomplete line: '%s'", buf);
            }
            break;
        } else {
            len += num;
            buf[len] = '\0';
            if (len + 1 >= bufsize) {
                bufsize *= 2;
                buf = putil_realloc(buf, bufsize);
            }
        }
    }

    return buf;
}

// Making this static instead of automatic makes Coverity happy.
static FILE *LogFP = NULL;

// Internal service routine for sorting the environment.
static int
_qsort_env(const void *p1, const void *p2)
{
    const char **lp = (const char **)p1;
    const char **rp = (const char **)p2;

    return strcmp(*lp, *rp);
}

/// The Unix version of the function which actually runs and audits the
/// child command. The child command is run via fork/exec/wait while
/// this process sticks around to become the "monitor".
/// The monitor waits for the child to exit and in the meantime it
/// processes audit reports sent from subordinate commands as they exit.
/// @param[in] exe      the path to this program (not the audited program)
/// @param[in] argv     a standard arg vector such as is passed to main()
/// @param[in] logfile  the name of a file to which log data is written, or NULL
/// @return 0 on success, nonzero otherwise
int
run_cmd(CCS exe, CS *argv, CCS logfile)
{
    CS path;
    pid_t childpid;
    struct timeval master_timeout, timeout;
    int sockmin = 3, sockmax = 0;
    int *listeners;
    unsigned long ports;
    fd_set listen_fds;
    fd_set master_read_fds;
    int sret;
    char *pdir;
    char *shlibdir = NULL;
    int sync_pipe[2];
    int wstat = 0;
    unsigned int i;

    path = argv[0];

    ports = prop_get_ulong(P_MONITOR_LISTENERS);
    TRY(listeners = malloc(ports * sizeof(int)), "malloc()");

    // If a logfile is requested, fork a "tee" and connect
    // stdout and stderr to it. This requires that we trust
    // tee to go away when its stdin is closed.
    // Note: we could put this on the child side of the fork
    // and see only the "real" build messages, but this way
    // is more compatible with Windows and potentially more
    // informative. Kind of mind-bending though.
    if (logfile) {
        CCS loglevel;

        loglevel = prop_get_str(P_SERVER_LOG_LEVEL);
        if (loglevel && !strcmp(loglevel, "OFF")) {
            if (!(LogFP = fopen(logfile, "w"))) {
                putil_syserr(2, logfile);
                exit(2);
            }
        } else {
            char *tcmd, *tsf;

            tsf = prop_is_true(P_LOG_TIME_STAMP) ? " --log-time-stamp" : "";
            if (asprintf(&tcmd, "%s --log-file \"%s\" --log-tee%s",
                    exe, logfile, tsf) >= 0) {
                if (!(LogFP = popen(tcmd, "w"))) {
                    putil_syserr(2, logfile);
                    exit(2);
                }
                FREE(tcmd);
            }
        }

        if (LogFP) {
            if (dup2(fileno(LogFP), fileno(stdout)) == -1) {
                putil_syserr(2, logfile);
                exit(2);
            }

            if (dup2(fileno(LogFP), fileno(stderr)) == -1) {
                putil_syserr(2, logfile);
                exit(2);
            }
        }

        // Show the initial command and start time iff output is to a logfile.
        if (vb_bitmatch(VB_STD)) {
            CCS cmdline;
            time_t now;

            cmdline = util_requote_argv(argv);
            fprintf(vb_get_stream(), "+ %s\n", cmdline);
            FREE(cmdline);
            time(&now);
            vb_printf(VB_ON, "STARTED: %s", util_strtrim(ctime(&now)));
        }
    }

    // This property allows the entire auditing process to be dummied out.
    // Simply run the specified command and return.
    if (prop_is_true(P_EXECUTE_ONLY)) {
        execvp(path, argv);
        putil_syserr(2, path);
        exit(2);
    }

    // The grandparent dir of this exe becomes the base where
    // we look for the preloaded library.
    if (!(pdir = putil_dirname(exe)) || !(shlibdir = putil_dirname(pdir))) {
        putil_syserr(2, "dirname(exe)");
        exit(2);
    }
    FREE(pdir);

    // Special case for truly "raw" output. There's no need
    // for a monitor process here; just inject and exec.
    if (shlibdir && prop_is_true(P_NO_MONITOR)) {
        _interposer_preload_on(AUDITOR, shlibdir);
        execvp(path, argv);
        putil_syserr(2, path);
        exit(2);
    }

    // This pipe is only used to ensure that the child doesn't get rolling
    // before the parent process is ready for its audits.
    TRY(pipe(sync_pipe) != -1, "pipe(sync_pipe)");

    {
        int so_reuse = 1;
        struct sockaddr_in addr;
        socklen_t addrlen;
        char *portstr;
        size_t len;
        u_short port;

        // Max chars needed to represent a 32-bit value plus ':' is 11.
        len = ports * 11;
        portstr = putil_calloc(len, 1);

        for (i = 0; i < ports; i++) {
            if ((listeners[i] = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
                putil_syserr(2, "socket()");
                exit(2);
            }

            if (setsockopt(listeners[i], SOL_SOCKET, SO_REUSEADDR,
                           &so_reuse, sizeof(so_reuse)) == SOCKET_ERROR) {
                putil_syserr(2, "SO_REUSEADDR");
                exit(2);
            }

            if (setsockopt(listeners[i], SOL_SOCKET, SO_REUSEPORT,
                           &so_reuse, sizeof(so_reuse)) == SOCKET_ERROR) {
                putil_syserr(2, "SO_REUSEPORT");
                exit(2);
            }

            addrlen = sizeof(addr);
            memset(&addr, 0, addrlen);
            addr.sin_family = PF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(0);

            if (bind(listeners[i], (struct sockaddr *)&addr, addrlen)) {
                putil_syserr(2, "bind()");
                exit(2);
            }

            if (getsockname(listeners[i], (struct sockaddr *)&addr, &addrlen)) {
                putil_syserr(2, "getsockname");
                exit(2);
            }

            port = ntohs(addr.sin_port);
            snprintf(endof(portstr), len - strlen(portstr), "%u:", port);
        }

        prop_set_str(P_MONITOR_PORT, portstr);
        FREE(portstr);
    }

    // Show exported properties just before fork/exec.
    if (vb_bitmatch(VB_TMP)) {
        system("env | grep -E '^(AO_|LD_)' | sort");
    }

    if ((childpid = fork()) < 0) {
        putil_syserr(2, "fork");
        exit(2);
    } else if (childpid == 0) {
        char sync_buf[2];
        struct sockaddr_in dest_addr;
        int nfd;

        //////////////////////////////// CHILD ///////////////////////////

        // Not needed on child side.
        for (i = 0; i < ports; i++) {
            fcntl(listeners[i], F_SETFD, fcntl(listeners[i], F_GETFD) | FD_CLOEXEC);
        }

        // We must wait till the parent has established its socket
        // before letting the build run. Otherwise we might try to
        // connect to it before it's ready.
// Pragma to suppress an apparently bogus gcc -fanalyzer complaint.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
        close(sync_pipe[1]);
        read(sync_pipe[0], sync_buf, 1);
        close(sync_pipe[0]);
#pragma GCC diagnostic pop

        // Turn on the auditor.
        if (shlibdir) {
            _interposer_preload_on(AUDITOR, shlibdir);
            FREE(shlibdir);
        }

        // Sorting the env before exec is not required but a nice frill.
        int env_size;
        for (env_size = 0; environ[env_size]; env_size++);
        char **child_environ = (char **)alloca((env_size + 1) * sizeof(void *));
        for (int j = 0; environ[j]; j++) {
            child_environ[j] = environ[j];
        }
        child_environ[env_size] = NULL;
        qsort(child_environ, env_size, sizeof(char *), _qsort_env);

        // Now actually run the command under control of the audit lib.
        execvpe(path, argv, child_environ);

        // Give an error if we were unable to exec at all.
        putil_die("%s: %s", path, strerror(errno));

        // The parent will be blocking on the first listening socket and needs
        // to hear from us on exec error. Send a special code telling
        // it to shut down gracefully.
        if ((nfd = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            putil_syserr(2, "socket()");
            exit(2);
        }

        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = PF_INET;
        dest_addr.sin_addr.s_addr = inet_addr(prop_get_str(P_MONITOR_HOST));
        dest_addr.sin_port = htons(strtoul(prop_get_str(P_MONITOR_PORT), NULL, 0));

        if ((connect(nfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))) == -1) {
            putil_syserr(2, "connect()");
            exit(2);
        }

// Pragma to suppress an apparently bogus gcc -fanalyzer complaint.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-fd-use-without-check"
        // Tell the parent to abort the mission.
        write(nfd, "!\n", 2);
#pragma GCC diagnostic pop

        close(nfd);

        _exit(2);
    }

    FREE(shlibdir);

    //////////////////////////////// PARENT //////////////////////////////

    _unix_sig_setup();

    // Perform any one-time initializations related to the monitor.
    mon_init();

    // This pipe is used as a belt-and-suspenders method for letting
    // the monitor know that the build is done.
    if (pipe(done_pipe) == -1) {
        putil_syserr(2, "pipe(done_pipe)");
        exit(2);
    }

    FD_ZERO(&master_read_fds);
    FD_ZERO(&listen_fds);

    // Add the read end of the self-pipe to the master set.
    FD_SET(done_pipe[0], &master_read_fds);
    sockmax = done_pipe[0];

    for (i = 0; i < ports; i++) {
        // Set up these sockets as listeners.
        if (listen(listeners[i], SOMAXCONN) == SOCKET_ERROR) {
            putil_syserr(2, "listen");
            exit(2);
        }

        // Add the listening sockets to the master set.
        FD_SET(listeners[i], &master_read_fds);

        // And to the listener set
        FD_SET(listeners[i], &listen_fds);

        // Keep track of the highest-numbered live socket.
        sockmax = (listeners[i] > sockmax) ? listeners[i] : sockmax;
    }

// Pragma to suppress an apparently bogus gcc -fanalyzer complaint.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
    // Alert the child that we're ready to roll.
    (void)close(sync_pipe[0]);
    (void)close(sync_pipe[1]);
#pragma GCC diagnostic pop

    master_timeout.tv_sec = prop_get_ulong(P_MONITOR_TIMEOUT_SECS);
    master_timeout.tv_usec = 0;

    // The select loop.
    // Some network gurus argue that select should never be used
    // with blocking descriptors, but that's what we do here. Never looked
    // into why they say so but this should be run past an expert.
    while (!doneflag) {
        int fd;
        fd_set read_fds;

        // Reset the timeout because some implementations of
        // select modify it. This could cause it to busy-wait
        // as the timeout becomes zero.
        timeout = master_timeout;

        // Initialize the read fdset to its top-of-loop state.
        read_fds = master_read_fds;

        sret = select(sockmax + 1, &read_fds, NULL, NULL, &timeout);

        if (sret == SOCKET_ERROR) {
            if (errno == EINTR) {
                continue;
            }
            putil_syserr(2, "select");
            exit(2);
        }

        for (i = 0; i < ports; i++) {
            int found = 0;

            if (FD_ISSET(listeners[i], &read_fds)) {
                SOCKET newfd;

                // Accept a new connection.
                if ((newfd = accept(listeners[i], NULL, NULL)) == INVALID_SOCKET) {
                    putil_syserr(2, "accept");
                    exit(2);
                }

                // Add it to the master set
                FD_SET(newfd, &master_read_fds);

                // Keep track of the highest numbered socket
                if (newfd > sockmax) {
                    sockmax = newfd;
                }

                found++;
            }

            // ????
            if (found) {
                continue;
            }
        }

        // This can be turned on with a signal.
        if (dumpflag) {
            mon_dump();
            dumpflag = 0;
        }

        // Run through existing connections looking for data
        for (fd = sockmin; fd <= sockmax; fd++) {
            CS buffer, buftmp, line;

            if (FD_ISSET(fd, &listen_fds) || !FD_ISSET(fd, &read_fds)) {
                continue;
            }

            buffer = _unix_read_available(fd);

            for (buftmp = buffer, line = util_strsep(&buftmp, "\n");
                     line && *line; line = util_strsep(&buftmp, "\n")) {

                if (!strcmp(line, DONE_TOKEN)) {
                    // If the top-level process has ended, we have
                    // to be finished.
                    vb_printf(VB_MON, "DONE: %lu", childpid);
                    doneflag = 1;
                } else {
                    unsigned monrc;
                    CCS winner;

                    monrc = mon_record(line, &ExitStatus, NULL, &winner);

                    if (monrc & MON_NEXT) {
                        // Nothing more to do - hit me again.
                    } else if (monrc & MON_ERR) {
                        // Nothing more to do - error already handled.
                    } else if (monrc & MON_CANTRUN) {
                        // This means the top-level process was unable
                        // to run at all.
                        doneflag = 1;
                        break;
                    } else if (monrc & MON_SOA) {
                        char ack[ACK_BUFFER_SIZE];

                        if (monrc & MON_RECYCLED) {
                            // Tell auditor we recycled so it can exit.
                            snprintf(ack, sizeof(ack), "%s\n", winner);
                        } else if (monrc & MON_STRICT) {
                            // Tell auditor we failed a requirement.
                            snprintf(ack, sizeof(ack), "%s\n", ACK_FAILURE);
                            ExitStatus = 3;
                        } else if (monrc & MON_AGG) {
                            // Tell auditor this command is aggregated.
                            snprintf(ack, sizeof(ack), "%s\n", ACK_OK_AGG);
                        } else {
                            // No special message - carry on.
                            snprintf(ack, sizeof(ack), "%s\n", ACK_OK);
                        }

                        if (util_send_all(fd, ack, strlen(ack), 0) == -1) {
                            putil_syserr(0, "send(ack)");
                        }

                        if (monrc & MON_TOP) {
                            if (Started) {
                                mon_ptx_end(ExitStatus, logfile);
                            }
                            mon_ptx_start();
                            Started = 1;
                        }
                    } else if (monrc & MON_EOA) {
                        // Nothing more to do - end processing handled elsewhere
                    } else {
                        putil_warn("unrecognized line '%s'", line);
                    }
                }
            }

            // We've reached EOF on a particular delivery;
            // close the socket, remove it from the select mask, and
            // return to the loop.
            close(fd);
            FD_CLR(fd, &master_read_fds);

            FREE(buffer);
        }

        // http_async_transfer(0);
    }

    // Wait for ending child, reap its exit code.
    if (waitpid(childpid, &wstat, 0) == -1) {
        putil_syserr(0, path);
        ExitStatus = 5;
    }
    if (WIFEXITED(wstat)) {
        if (ExitStatus == 0) {
            ExitStatus = WEXITSTATUS(wstat);
        }
    } else {
        if (WIFSIGNALED(wstat)) {
            putil_error("%s: %s%s", path, strsignal(WTERMSIG(wstat)),
                        WCOREDUMP(wstat) ? " (coredump)" : "");
        }
        ExitStatus = 2;
    }

    mon_ptx_end(ExitStatus, logfile);

    mon_fini();

    for (i = 0; i < ports; i++) {
        if (close(listeners[i]) == SOCKET_ERROR) {
            putil_syserr(0, "close(socket)");
        }
    }

    if (LogFP) {
        (void)fclose(LogFP);
    }

    return ExitStatus;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
