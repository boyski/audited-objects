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
/// @brief Provides the Windows-specific code to kick off the top-level
/// audited command and monitor it and its children, receiving
/// "audit packets", processing them, and uploading data to the server.
/// The analogous Unix code is in unix.c.

#include "AO.h"

#include "ACK.h"
#include "CA.h"
#include "HTTP.h"
#include "MON.h"
#include "PROP.h"
#include "UW.h"

#include "LibAO.H"

#include <process.h>
#include <winsock2.h>

static int doneflag = 0;
static int ExitStatus = 0;
static int Started = 0;

static CS
_read_available(SOCKET fd)
{
    CS buf;
    ssize_t bufsize = 1024, len, num;

    buf = putil_malloc(bufsize);

    for (len = 0, buf[0] = '\0'; ;) {
	num = recv(fd, buf + len, bufsize - len - 1, 0);
	if (num < 0) {
	    putil_syserr(2, "read");
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

/// The Windows version of the function which actually runs and audits
/// the child command. The child command is run via CreateProcess while
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
    DWORD childpid;
    unsigned lineno = 0;
    int reuseaddr = 1;
    struct timeval master_timeout, timeout;
    int64_t last_heartbeat, heartbeat_interval;
    long session_timeout;
    SOCKET asock, sockmin = 0, sockmax = 0;
    SOCKET listeners[LISTENERS];
    SOCKET *listeners;
    unsigned long ports;
    fd_set listen_fds;
    fd_set master_read_fds;
    int sret;
    char prgpath[MAX_PATH];
    char dllpath[MAX_PATH];
    PROCESS_INFORMATION pi = {0};
    STARTUPINFO si = {0};
    CS cmdline = NULL;
    HINSTANCE auditor_handle;
    INJECTOR LDPInjectW;
    DWORD wfso;
    unsigned int i;

    memset(&pi, 0, sizeof(pi));
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    path = argv[0];

    ports = prop_get_ulong(P_MONITOR_LISTENERS);
    listeners = putil_malloc(ports * sizeof(int));

    // Try in advance to find the executable we're going to run.
    // Partly to see if it's an exe or a script and partly
    // to save the injector the work of searching later.
    if (!SearchPath(NULL, path, ".exe", charlen(prgpath), prgpath, NULL)) {
	if (!SearchPath(NULL, path, ".bat",
			charlen(prgpath), prgpath, NULL)) {
	    prgpath[0] = '\0';
	}
    }

    if (prgpath[0]) {
	if (endswith(prgpath, ".exe")) {
	    path = argv[0] = (const CS)prgpath;
	    cmdline = util_requote_argv(argv);
	} else {
	    CS cmd, p;

	    // Can't CreateProcess a script - we need an explicit shell.
	    p = util_requote_argv(argv);
	    cmd = putil_getenv("COMSPEC");
	    (void)trio_asprintf(&cmdline, "%s /c %s", cmd, p);
	    putil_free(p);
	    path = cmd;
	}
    } else {
	cmdline = util_requote_argv(argv);
    }

    // If a logfile is requested, fork a "tee" and connect
    // stdout and stderr to it. This requires that we trust
    // tee to go away when its stdin is closed.
    if (logfile) {
	CCS loglevel;

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
	    if ((_dup2(_fileno(logfp), _fileno(stdout)) == -1)) {
		putil_syserr(2, logfile);
	    }

	    if ((_dup2(_fileno(logfp), _fileno(stderr)) == -1)) {
		putil_syserr(2, logfile);
	    }
	}

	// Show the initial command and start time iff output is to a logfile.
	if (vb_bitmatch(VB_STD)) {
	    time_t now;

	    fprintf(vb_get_stream(), "+ %s\n", cmdline);
	    time(&now);
	    vb_printf(VB_ON, "STARTED: %s", util_strtrim(ctime(&now)));
	}
    }

    // This property allows the entire auditing process to be dummied out;
    // we simply run the specified command, wait for it, and return.
    if (prop_is_true(P_EXECUTE_ONLY)) {
	if (CreateProcess(NULL, cmdline, NULL, NULL, FALSE,
			  0, NULL, NULL, &si, &pi) == 0) {
	    putil_win32err(2, GetLastError(), path);
	}
	wfso = WaitForSingleObject(pi.hProcess, INFINITE);
	if (wfso == WAIT_OBJECT_0) {
	    DWORD drc = 0;

	    if (GetExitCodeProcess(pi.hProcess, &drc)) {
		ExitStatus = (int)drc;
	    } else {
		putil_win32err(0, GetLastError(), "GetExitCodeProcess");
		ExitStatus = 2;
	    }
	} else if (wfso == WAIT_FAILED) {
	    ExitStatus = 5;
	}
	return ExitStatus;
    }

    // Prior to Windows 7 there's a problem injecting DLLs into
    // managed code. This is part of a workaround for that.
    // The rest is in LibWin.cpp. Not yet fully working.
    {
	OSVERSIONINFO oviVersion;
	ZeroMemory(&oviVersion, sizeof(OSVERSIONINFO));
	oviVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if(GetVersionEx(&oviVersion) && oviVersion.dwMajorVersion <= 6) {
	    if (putil_getenv("COR_ENABLE_PROFILING") ||
		    putil_getenv("COR_PROFILER")) {
		putil_warn("Conflict: competing profiler for managed code");
	    } else {
		putil_putenv("COR_ENABLE_PROFILING=1");
		putil_putenv("COR_PROFILER=cpci.Profiler");
	    }
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
	    if ((listeners[i] = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, 0)) == INVALID_SOCKET) {
		putil_win32err(2, WSAGetLastError(), "socket()");
	    }

	    if (setsockopt(listeners[i], SOL_SOCKET, SO_REUSEADDR,
			   (const char *)&reuseaddr,
			   sizeof(reuseaddr)) == SOCKET_ERROR) {
		putil_win32err(2, WSAGetLastError(), "SO_REUSEADDR");
	    }

	    addrlen = sizeof(addr);
	    memset(&addr, 0, addrlen);
	    addr.sin_family = PF_INET;
	    addr.sin_addr.s_addr = INADDR_ANY;
	    addr.sin_port = htons(0);

	    if (bind(listeners[i], (struct sockaddr *)&addr, addrlen) != SOCKET_ERROR) {
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

    last_heartbeat = time(NULL);

    // Do an explicit runtime load of the auditing DLL, not because
    // the monitor process needs to be audited (it doesn't) but so we
    // can access the injection routine within it.
    auditor_handle = LoadLibrary(AUDITOR);
    if (auditor_handle == NULL) {
	putil_win32err(2, GetLastError(), "LoadLibrary(" AUDITOR ")");
    }

    // According to an article in MSDN, searching PATH for DLLs is
    // by far the most expensive part of loading them. So we go to
    // some trouble to search the path once and work only with
    // the fully qualified pathname from there on.
    if (!GetModuleFileName(auditor_handle, dllpath, charlen(dllpath))) {
	putil_win32err(2, GetLastError(), "GetModuleFileName");
    }

    // Run the actual command. Create it in a suspended state so we
    // can mess with it before it runs.
    if (CreateProcess(NULL, cmdline, NULL, NULL, FALSE,
		      CREATE_SUSPENDED, NULL, NULL, &si, &pi) == 0) {
	putil_win32err(2, GetLastError(), path);
    }

    // Inject the auditing DLL, which will do most of its work via
    // the PreMain() init function. The injection function itself
    // is in the DLL, so we have to look it up there.
    if ((LDPInjectW = (INJECTOR)GetProcAddress(auditor_handle, "LDPInjectW"))) {
	int widelen;
	LPWSTR pathW;

	widelen = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
	pathW = (LPWSTR) _alloca(widelen * sizeof(WCHAR));
	widelen = MultiByteToWideChar(CP_ACP, 0, path, -1, pathW, widelen);
	if (!LDPInjectW(&pi, pathW, dllpath, PREMAIN)) {
	    int last;

	    last = GetLastError();
	    TerminateProcess(pi.hProcess, 42);
	    vb_printf(VB_TMP, "path='%s' dllpath='%s'", path, dllpath);
	    putil_win32err(2, last, dllpath);
	}
    } else {
	putil_win32err(2, GetLastError(), "GetProcAddress(LDPInjectW)");
    }

    // ... and then start the program with our DLL now mapped in.
    if (ResumeThread(pi.hThread) == -1) {
	int last;

	last = GetLastError();
	TerminateProcess(pi.hProcess, 42);
	putil_win32err(2, last, dllpath);
    }

    childpid = pi.dwProcessId;

    // Perform any one-time initializations related to upload.
    mon_init();

    FD_ZERO(&master_read_fds);
    FD_ZERO(&listen_fds);

    for (i = 0; i < ports; i++) {
	// Set up these sockets as listeners.
	if (listen(listeners[i], SOMAXCONN) == SOCKET_ERROR) {
	    putil_win32err(2, WSAGetLastError(), "listen");
	}

	// Add the listening sockets to the master set.
	FD_SET(listeners[i], &master_read_fds);

	// And to the listener set
	FD_SET(listeners[i], &listen_fds);

	// Keep track of the highest-numbered live socket.
	sockmax = (listeners[i] > sockmax) ? listeners[i] : sockmax;
    }

    // The select loop.
    // Some network gurus argue that select should never be used
    // with blocking descriptors, but that's what we do here. Never looked
    // into why they say so but this should be run past an expert.
    while (!doneflag) {
	fd_set read_fds;

	// We have to reset the timeout because some implementations
	// of select modify it. This could cause it to busy-wait
	// as the timeout becomes zero.
	// For consistency with Unix - not needed on Windows.
	timeout = master_timeout;

	// Initialize the read fdset to its top-of-loop state.
	read_fds = master_read_fds;

	sret = select(-1, &read_fds, NULL, NULL, &timeout);

	if (sret == SOCKET_ERROR) {
	    putil_win32err(0, WSAGetLastError(), "select");
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

	    // Whenever select has timed out, sneak a quick peek at
	    // the child process to make sure it hasn't aborted.
	    if (sret == 0) {
		DWORD wfso = WaitForSingleObject(pi.hProcess, 0);

		if (wfso == WAIT_OBJECT_0) {
		    doneflag = 1;
		    break;
		}
		continue;
	    }
	}

	for (i = 0; i < ports; i++) {
	    int found = 0;

	    if (FD_ISSET(listeners[i], &read_fds)) {
		SOCKET newfd;

		// Accept a new connection.
		if ((newfd = accept(listeners[i], NULL, NULL)) == INVALID_SOCKET) {
		    putil_win32err(2, WSAGetLastError(), "accept");
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

	// Run through existing connections looking for data.
	// For some reason sockets seem to start at a high number (1000?)
	// which means we waste time looping from 0 to the first one.
	// Might there be a better (higher) place to start than 0?
	for (asock = sockmin; asock <= sockmax; asock++) {
	    CS buffer, buftmp, line;

	    if (FD_ISSET(asock, &listen_fds) || !FD_ISSET(asock, &read_fds)) {
		continue;
	    }

	    buffer = _read_available(asock);

	    for (buftmp = buffer, line = util_strsep(&buftmp, "\n");
		     line && *line; line = util_strsep(&buftmp, "\n")) {

		if (!strcmp(line, DONE_TOKEN)) {
		    // If the top-level process has ended, we have
		    // to be finished.
		    // NOTE: This really can't happen on Windows but we
		    // like the code to look similar.
		    vb_printf(VB_MON, "DONE: %lu", childpid);
		    doneflag = 1;
		} else {
		    unsigned monrc;
		    CCS winner;
		    DWORD cmdpid = -1;

		    monrc = mon_record(line, &ExitStatus, &cmdpid, &winner);

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

			if (util_send_all(asock, ack, strlen(ack), 0) == -1) {
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
			if (monrc & MON_TOP) {
			    doneflag = (cmdpid == childpid);
			}
		    } else {
			putil_warn("unrecognized line '%s'", line);
		    }
		}
	    }

	    // We've reached EOF on a particular delivery;
	    // close the socket, remove it from the select mask, and
	    // return to the loop.
	    if (shutdown(asock, 2) == SOCKET_ERROR) {
		putil_win32err(0, GetLastError(), "shutdown()");
	    }
	    if (closesocket(asock) == SOCKET_ERROR) {
		putil_win32err(0, GetLastError(), "closesocket()");
	    }
	    FD_CLR(asock, &master_read_fds);

	    putil_free(buffer);
	}

	http_async_transfer(0);
    }

    // Wait for ending child, reap its exit code.
    wfso = WaitForSingleObject(pi.hProcess, INFINITE);
    if (wfso == WAIT_OBJECT_0) {
	DWORD drc = 0;

	if (!ExitStatus) {
	    if (GetExitCodeProcess(pi.hProcess, &drc)) {
		ExitStatus = (int)drc;
	    } else {
		putil_win32err(2, GetLastError(), "GetExitCodeProcess");
	    }
	}
    } else if (wfso == WAIT_FAILED) {
	putil_win32err(0, GetLastError(), path);
	if (!ExitStatus) {
	    ExitStatus = 5;
	}
    }

    mon_ptx_end(ExitStatus, logfile);

    mon_fini();

    for (i = 0; i < ports; i++) {
	if (closesocket(listeners[i]) == SOCKET_ERROR) {
	    putil_win32err(0, GetLastError(), "closesocket()");
	}
    }

    util_socket_lib_fini();

    putil_free(cmdline);

    return ExitStatus;
}
