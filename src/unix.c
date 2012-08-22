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
/// @brief Provides the Unix-specific code to kick off the top-level
/// audited command and monitor it and its children, receiving
/// "audit packets", processing them, and uploading data to the server.
/// The analogous Windows code is in win.c.

#include "AO.h"

#include "ACK.h"
#include "CA.h"
#include "HTTP.h"
#include "MON.h"
#include "PROP.h"
#include "UW.h"

#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Interposer/libinterposer.h"

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

// Request the maximum number of file descriptors allowed by the kernel.
static void
_maximize_fds(void)
{
    struct rlimit rlims = {0, 0};

    if (getrlimit(RLIMIT_NOFILE, &rlims)) {
	putil_syserr(0, "getrlimit()");
	return;
    }

    // getrlimit seems to produce a bogus value for rlim_max on OS X.
    // No idea why but this is what they recommend.
#if defined(__APPLE__)
    rlims.rlim_cur = OPEN_MAX < rlims.rlim_max ? OPEN_MAX : rlims.rlim_max;
#else /*!APPLE*/
    rlims.rlim_cur = rlims.rlim_max;
#endif /*APPLE*/

    if (setrlimit(RLIMIT_NOFILE, &rlims)) {
	putil_syserr(0, "setrlimit()");
    }
}

// Used to alert the monitor that the top-level child has ended.
static void
_sigchld(int signum)
{
    UNUSED(signum);
    doneflag = 1;
    if (write(done_pipe[1], DONE_TOKEN, strlen(DONE_TOKEN)) == -1) {
	putil_syserr(2, "write(done_pipe[1])");
    }
    if (write(done_pipe[1], "\n", 1) == -1) {
	putil_syserr(2, "write(done_pipe[1])");
    }
    close(done_pipe[1]);
}

// Used to dump debug data from a signal.
static volatile sig_atomic_t dumpflag;
static void
_sigusr1(int signum)
{
    dumpflag = signum;
}

static void
_sig_setup(void)
{
    struct sigaction sa;

    // Alert the monitor loop when the child is no longer alive.
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = _sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_RESETHAND;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
	putil_syserr(2, "sigaction(SIGCHLD)");
    }

    // Debug feature: dump the current audit DB upon catching a signal.
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = _sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
	putil_syserr(2, "sigaction(SIGUSR1)");
    }
}

static CS
_read_available(int fd)
{
    CS buf;
    ssize_t bufsize = 1024, len, num;

    buf = putil_malloc(bufsize);

    for (len = 0, buf[0] = '\0'; ;) {
	num = read(fd, buf + len, bufsize - len - 1);
	if (num < 0) {
	    if (errno != EAGAIN) {
		putil_syserr(2, "read");
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
static FILE *logfp = NULL;

/// The Unix version of the function which actually runs and audits the
/// child command. The child command is run via fork/exec/wait while
/// this process sticks around to become the "monitor".
/// The monitor waits for the child to exit and in the meantime it
/// processes audit reports sent from exiting subordinate commands.
/// @param[in] exe      the path to this program (not the audited program)
/// @param[in] argv     a standard arg vector such as is passed to main()
/// @param[in] logfile  the name of a file to which log data is written, or NULL
/// @return 0 on success, nonzero otherwise
int
run_cmd(CCS exe, CS *argv, CCS logfile)
{
    CS path;
    pid_t childpid;
    int reuseaddr = 1;
    struct timeval master_timeout, timeout;
    int64_t session_timeout, last_heartbeat, heartbeat_interval;
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
    listeners = putil_malloc(ports * sizeof(int));

    // If a logfile is requested, fork a "tee" and connect
    // stdout and stderr to it. This requires that we trust
    // tee to go away when its stdin is closed.
    // Note: we could put this on the child side of the fork
    // and see only the "real" build messages, but this way
    // is more compatible with Windows and potentially more
    // informative. Kind of mind-bending though.
    if (logfile) {
	const char *loglevel;

	loglevel = prop_get_str(P_SERVER_LOG_LEVEL);
	if (loglevel && !strcmp(loglevel, "OFF")) {
	    if (!(logfp = fopen(logfile, "w"))) {
		putil_syserr(2, logfile);
	    }
	} else {
	    char *tcmd, *tsf;

	    tsf = prop_is_true(P_LOG_TIME_STAMP) ? " --log-time-stamp" : "";
	    if (asprintf(&tcmd, "%s --log-file \"%s\" --log-tee%s",
		    exe, logfile, tsf) >= 0) {
		if (!(logfp = popen(tcmd, "w"))) {
		    putil_syserr(2, logfile);
		}
		putil_free(tcmd);
	    }
	}

	if (logfp) {
	    if ((dup2(fileno(logfp), fileno(stdout)) == -1)) {
		putil_syserr(2, logfile);
	    }

	    if ((dup2(fileno(logfp), fileno(stderr)) == -1)) {
		putil_syserr(2, logfile);
	    }
	}

	// Show the initial command and start time iff output is to a logfile.
	if (vb_bitmatch(VB_STD)) {
	    CCS cmdline;
	    time_t now;

	    cmdline = util_requote_argv(argv);
	    fprintf(vb_get_stream(), "+ %s\n", cmdline);
	    putil_free(cmdline);
	    time(&now);
	    vb_printf(VB_ON, "STARTED: %s", util_strtrim(ctime(&now)));
	}
    }

    // This property allows the entire auditing process to be dummied out;
    // we simply run the specified command, wait for it, and return.
    if (prop_is_true(P_EXECUTE_ONLY)) {
	if ((childpid = fork()) < 0) {
	    putil_syserr(2, "fork");
	} else if (childpid == 0) {
	    execvp(path, argv);
	    putil_syserr(2, path);
	} else {
	    if (waitpid(childpid, &wstat, 0) == -1) {
		putil_syserr(0, path);
		ExitStatus = 5;
	    } else if (WIFEXITED(wstat)) {
		ExitStatus = WEXITSTATUS(wstat);
	    } else {
		ExitStatus = 2;
	    }
	    return ExitStatus;
	}
    }

    // The heartbeat interval is the number of seconds allowed
    // before we send a server heartbeat ping to keep the server
    // HTTP session alive.  The Tomcat default is 30 minutes
    // but there's no standard default, so we set the default
    // expicitly in the server's web.xml file. This allows us to
    // assume that default via HTTP_SESSION_TIMEOUT_SECS_DEFAULT.

    master_timeout.tv_sec = prop_get_ulong(P_MONITOR_TIMEOUT_SECS);
    master_timeout.tv_usec = 0;
    session_timeout = prop_get_ulong(P_SESSION_TIMEOUT_SECS);
    if (session_timeout) {
	if ((session_timeout / 4 < master_timeout.tv_sec)) {
	    master_timeout.tv_sec = session_timeout / 4;
	}
	heartbeat_interval = session_timeout / 2;
    } else {
	heartbeat_interval = HTTP_SESSION_TIMEOUT_SECS_DEFAULT / 2;
    }

    // This is really only required on Windows.
    util_socket_lib_init();

    // The grandparent dir of this exe becomes the base where
    // we look for the preloaded library.
    if (!(pdir = putil_dirname(exe)) || !(shlibdir = putil_dirname(pdir))) {
	putil_syserr(2, "dirname(exe)");
    }
    putil_free(pdir);

    // Special case for truly "raw" output. There's no need
    // for a monitor process here; just inject and exec.
    if (shlibdir && prop_is_true(P_NO_MONITOR)) {
	libinterposer_preload_on(AUDITOR, shlibdir);
	execvp(path, argv);
	putil_syserr(2, path);
    }

    // This pipe will be used to ensure that the child doesn't get rolling
    // before the parent process is ready for its audits.
    if (pipe(sync_pipe) == -1) {
	putil_syserr(2, "pipe(sync_pipe)");
    }

    {
	struct sockaddr_in addr;
	socklen_t addrlen;
	char *portstr;
	size_t len;
	u_short port;

	// Max digits needed to represent a 32-bit value plus ':' is 11.
	len = ports * 11;
	portstr = putil_calloc(len, 1);

	for (i = 0; i < ports; i++) {
	    if ((listeners[i] = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		putil_syserr(2, "socket()");
	    }

	    if (setsockopt(listeners[i], SOL_SOCKET, SO_REUSEADDR,
			   &reuseaddr, sizeof(reuseaddr)) == SOCKET_ERROR) {
		putil_syserr(2, "SO_REUSEADDR");
	    }

	    addrlen = sizeof(addr);
	    memset(&addr, 0, addrlen);
	    addr.sin_family = PF_INET;
	    addr.sin_addr.s_addr = INADDR_ANY;
	    addr.sin_port = htons(0);

	    if (bind(listeners[i], (struct sockaddr *)&addr, addrlen)) {
		putil_syserr(2, "bind()");
	    }

	    if (getsockname(listeners[i], (struct sockaddr *)&addr, &addrlen)) {
		putil_syserr(2, "getsockname");
	    }

	    port = ntohs(addr.sin_port);
	    snprintf(endof(portstr), len - strlen(portstr), "%u:", port);
	}

	prop_override_str(P_MONITOR_PORT, portstr);
	putil_free(portstr);
    }

    if ((childpid = fork()) < 0) {
	putil_syserr(2, "fork");
    } else if (childpid == 0) {
	char sync_buf[2];
	struct sockaddr_in dest_addr;
	int nfd;

	/*****************************************************************
	 * CHILD
	 *****************************************************************/

	// Not needed on child side.
	for (i = 0; i < ports; i++) {
	    fcntl(listeners[i], F_SETFD, fcntl(listeners[i], F_GETFD) | FD_CLOEXEC);
	}

	// We must wait till the parent has established its socket
	// before letting the build run. Otherwise we might try to
	// connect to it before it's ready.
	close(sync_pipe[1]);
	read(sync_pipe[0], sync_buf, 1);
	close(sync_pipe[0]);

	// Turn on the auditor.
	if (shlibdir) {
	    libinterposer_preload_on(AUDITOR, shlibdir);
	    putil_free(shlibdir);
	}

	// Run the command under control of the audit lib.
	execvp(path, argv);

	// Give an error if we were unable to exec at all.
	putil_die("%s: %s", path, strerror(errno));

	// The parent will be blocking on the first listening socket and needs
	// to hear from us on exec error. Send a special code telling
	// it to shut down gracefully.
	if ((nfd = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
	    putil_syserr(2, "socket()");
	}

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = PF_INET;
	dest_addr.sin_addr.s_addr = inet_addr(prop_get_str(P_MONITOR_HOST));
	dest_addr.sin_port = htons(strtoul(prop_get_str(P_MONITOR_PORT), NULL, 0));

	if ((connect(nfd, (struct sockaddr *)&dest_addr,
		     sizeof(struct sockaddr))) == -1) {
	    putil_syserr(2, "connect()");
	}

	// Tell the parent to abort the mission.
	write(nfd, "!\n", 2);

	close(nfd);

	_exit(2);
    }

    putil_free(shlibdir);

    /*********************************************************************
     * PARENT
     *********************************************************************/

    _sig_setup();

    last_heartbeat = time(NULL);

    // Bump the number of allowed file descriptors to the maximum,
    // because the monitor is likely to use a fair number. There
    // is no real bound on fds in use since the monitor may behave
    // asynchronously with respect to both audited child processes
    // and HTTP connections to the server.
    _maximize_fds();

    // Perform any one-time initializations related to the monitor.
    mon_init();

    // This pipe is used as a belt-and-suspenders method for letting
    // the monitor know that the build is done.
    if (pipe(done_pipe) == -1) {
	putil_syserr(2, "pipe(done_pipe)");
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
	}

	// Add the listening sockets to the master set.
	FD_SET(listeners[i], &master_read_fds);

	// And to the listener set
	FD_SET(listeners[i], &listen_fds);

	// Keep track of the highest-numbered live socket.
	sockmax = (listeners[i] > sockmax) ? listeners[i] : sockmax;
    }

    // Alert the child that we're ready to roll.
    close(sync_pipe[0]);
    close(sync_pipe[1]);

    // The select loop.
    // Some network gurus argue that select should never be used
    // with blocking descriptors, but that's what we do here. Never looked
    // into why they say so but this should be run past an expert.
    while (!doneflag) {
	int fd;
	fd_set read_fds;

	// We have to reset the timeout because some implementations
	// of select modify it. This could cause it to busy-wait
	// as the timeout becomes zero.
	timeout = master_timeout;

	// Initialize the read fdset to its top-of-loop state.
	read_fds = master_read_fds;

	sret = select(sockmax + 1, &read_fds, NULL, NULL, &timeout);

	if (sret == SOCKET_ERROR) {
#if defined(EINTR)
	    if (errno == EINTR) {
		continue;
	    }
#endif	/*!EINTR*/
	    putil_syserr(2, "select");
	} else {
	    // We like to ping the server once in a while, partly
	    // to make sure it's still there but primarily to keep
	    // its session alive.
	    if (prop_has_value(P_SERVER)) {
		int64_t now;

		now = time(NULL);
		if ((now - last_heartbeat) >= heartbeat_interval) {
		    http_heartbeat(now - last_heartbeat);
		    last_heartbeat = now;
		}
	    }

	    // Continue on select timeout.
	    if (sret == 0) {
		continue;
	    }
	}

	for (i = 0; i < ports; i++) {
	    int found = 0;

	    if (FD_ISSET(listeners[i], &read_fds)) {
		SOCKET newfd;

		// Accept a new connection.
		if ((newfd = accept(listeners[i], NULL, NULL)) == INVALID_SOCKET) {
		    putil_syserr(2, "accept");
		}

		// Add it to the master set
		FD_SET(newfd, &master_read_fds);

		// Keep track of the highest numbered socket
		if (newfd > sockmax) {
		    sockmax = newfd;
		}

		found++;
	    }

	    if (found)
		continue;
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

	    buffer = _read_available(fd);

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

	    putil_free(buffer);
	}

	http_async_transfer(0);
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
#if defined(__hpux)
	    // HACK - no strsignal on HPUX 11.00
	    putil_error("%s: %s%s", path, "",
			WCOREDUMP(wstat) ? " (coredump)" : "");
#else	/*__hpux*/
	    putil_error("%s: %s%s", path, strsignal(WTERMSIG(wstat)),
			WCOREDUMP(wstat) ? " (coredump)" : "");
#endif	/*__hpux*/
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

    util_socket_lib_fini();

    return ExitStatus;
}
