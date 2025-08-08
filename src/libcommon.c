// Copyright (c) 2005-2025 David Boyce.  All rights reserved.

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
/// @brief The part of the auditor library which is common across platforms.
/// Combines with libunix.c to form the auditing shared object. There used
/// to be a LibWin.cpp too but since Windows support was dropped the split
/// is merely historical.

#include "ACK.h"

#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DONE                            "{DONE}\n"

// These may require some fancy stepping in order to maintain
// thread safety within a potentially multithreaded host program.
// The general strategy is to initialize static data once during
// the single-threaded constructor and treat them as read-only
// afterwards but that may not always be an option.
static unsigned long Depth;
static ca_o CurrentCA;
static int AuditFD = -1;
static void *IgnorePathRE;
static void *IgnoreProgRE;

/// The socket through which all communication to the monitor takes place.
SOCKET ReportSocket = INVALID_SOCKET;

// This static flag indicates whether the auditor is active or quiescent.
// The default kind of activation is when the auditor is turned on from
// process start to process end. The other activation mode is when a long-
// running process, typically an IDE, may run with the auditor present
// but inactive during editing work but then turn it on, do a build,
// and turn it back off. This latter mode is more complex, less well
// tested, not yet documented, and largely hypothetical but it would be
// nice if it could be made to work reliably.
// Actually even that 'request mode' has two variants. One is when we
// match an activation RE internally and turn ourselves on, so to speak.
// The other is when the IDE makes an explicit call saying "start
// auditing now". Neither is well tested though.
static enum {
    LIBAO_INACTIVE,             ///< The auditor is not active.
    LIBAO_ACTIVE_BY_DEFAULT,    ///< The auditor is active throughout.
    LIBAO_ACTIVE_BY_REQUEST,    ///< The auditor was turned on by request.
} Activated = LIBAO_INACTIVE;

/// From each audited command the auditor library must "steal"
/// one file descriptor in order to send the audit packet back
/// to the monitor. We try not to take one of the "popular"
/// (low-numbered) ones but we don't want anything over 256 either
/// in case of low ceilings, and we also want to stay away from
/// 256,255,etc in case some clever tool likes to start at the
/// top and work downwards. So we begin by using an ordinary,
/// undistinguished descriptor such as this one.
#define PREFERRED_FD            212

/// A flag to _audit_end() indicating that we do not expect to exit.
#define NOT_EXITING             0

/// A flag to _audit_end() indicating that we're about to exit.
#define EXITING                 1

static void _audit_end(CCS, int, long);

/// Private API to ask whether the auditor is globally active.
/// @return true if so activated
static int
_auditor_isActiveByDefault()
{
    return Activated == LIBAO_ACTIVE_BY_DEFAULT;
}

/// Private API to ask whether the auditor is was activated by request.
/// @return true if so activated
static int
_auditor_isActiveByRequest()
{
    return Activated == LIBAO_ACTIVE_BY_REQUEST;
}

/// Private API to ask whether the auditor is currently active.
/// @return true if activated
static int
_auditor_isActive()
{
    return Activated != LIBAO_INACTIVE;
}

/// Private API to make the auditor globally active.
static void
_auditor_setActiveByDefault(void)
{
    Activated = LIBAO_ACTIVE_BY_DEFAULT;
}

/// Public API to request temporary auditor activation.
void
auditor_setActive(void)
{
    Activated = LIBAO_ACTIVE_BY_REQUEST;
}

/// Public API to make the auditor inactive.
void
auditor_setInactive(void)
{
    Activated = LIBAO_INACTIVE;
}

// Internal service routine. There are certain files we truly don't
// care about and just want to ignore as early as possible.
static int
_ignore_path(const char *path)
{
    int rc = 1;
    const char *pend;

    pend = endof(path);

    if (!strncmp(path, "/proc/", 6) || !strncmp(path, "/xfn/", 5) ||
        !strncmp(path, "/dev/", 5) || !strncmp(path, "/devices/", 9)) {
        // Typical "special" Unix file-like items.
    } else if (strstr(path, "/.ccache/")) {
        // There's no good reason to use ccache with AO but try to
        // behave reasonably if it happens anyway.
#if defined(linux)
    } else if (!strcmp(path, "/etc/mtab") || !strcmp(path, "/proc/mounts")) {
        // On Linux all processes seem to open /etc/mtab.
        // And we ourselves need to open /proc/mounts.
#elif defined(__APPLE__)
    } else if (strstr(path, "-Tmp-")) {
        // There's a lot of screwy stuff going on under /private/var/...
        // in OS X 10.5. No idea what these files are but they certainly
        // appear to be temp files based on their names.
    } else if (!strncmp(path, "/cores/", 7)) {
        // Vide infra. Linux and MacOS may put core dumps in a path like this.
#endif  // __APPLE__
    } else if ((pend - 5) >= path && !strcmp(pend - 5, "/core")) {
        // Nothing more embarrassing than dumping core (esp. if it's
        // our fault) and then uploading the core file ...
        // IDEA but it may be better to skip these only at upload time
        // (i.e. record the action but don't save the result).
        // There are legitimate operations which *read* a core file.
    } else if (util_is_tmp(path)) {
        // We used to not trust this, on the theory that data is often
        // communicated between processes in the form of temp files,
        // such as a compiler leaving a .s file in /tmp for the
        // assembler to process and remove. But it was enabled and
        // the test suite still passes, so left in at least until
        // it causes trouble. If we stop filtering tmp files here we
        // need to start filtering them at upload time.
    } else if (!strncmp(path, "/tmp/sh", 7) && atoi(path + 7) > 0) {
        // The shell tends to keep here-documents in a file called
        // "/tmp/shnnnnn" where "nnnnn" is based on the pid.
    } else if (IgnorePathRE && re_match__(IgnorePathRE, path)) {
        // User-configurable set of ignorable files. We may
        // eventually want to shift the hard-wired patterns
        // above to this model.
    } else {
        rc = 0;
    }

    return rc;
}

// Internal service routine. Called from intercepted system calls
// to register a file access (read, write, link, unlink, etc).
static void
_pa_record(CCS call, CCS path, CCS extra, int fd, op_e op)
{
    pn_o pn = NULL;
    ps_o ps;
    pa_o pa;

    if (!_auditor_isActive()) {
        vb_printf(VB_REC, "not active: %c,%s,%s", op, call, path);
        return;
    }

    pn = pn_new(path, 1);
    path = pn_get_abs(pn);

    if (_ignore_path(path)) {
        // We needed to derive a fully qualified path before doing this check.
        vb_printf(VB_REC, "ignoring: %c,%s,%s", op, call, path);
        pn_destroy(pn);
    } else if (!CurrentCA) {
        // This was moved down to after _ignore_path() to avoid
        // certain funky loops, such as when ao is run under the OS X
        // malloc debugger which writes a malloc_history file into /tmp
        // during exit processing.
        putil_int("PA after EOA: call=%s pid=%lu path=%s", call, (unsigned long)getpid(), path);
        pn_destroy(pn);
    } else {
        vb_printf(VB_REC, "recording: %c,%s,%s", op, call, path);

        pa = pa_new();
        pa_set_op(pa, op);
        pa_set_call(pa, call);
        pa_set_pid(pa, ca_get_cmdid(CurrentCA));
        pa_set_ppid(pa, ca_get_pcmdid(CurrentCA));
        pa_set_depth(pa, ca_get_depth(CurrentCA));
        pa_set_pccode(pa, ca_get_pccode(CurrentCA));
        pa_set_ccode(pa, ca_get_ccode(CurrentCA));
        pa_set_fd(pa, fd);

        // Only store the thread id for a threaded program (on Windows
        // they're all threaded). We believe 0 to be an invalid thread
        // id which is thus used to indicate "unthreaded".
        if (pthread_self_real) {
            pa_set_tid(pa, (unsigned long)pthread_self_real());
        } else {
            pa_set_tid(pa, 0);
        }

        ps = ps_new();
        ps_set_pn(ps, pn);

        // If this was a link op we may have a 2nd path to remember.
        if (extra) {
            if (op == OP_SYMLINK) {
                ps_set_target(ps, extra);
            } else {
                ps_set_pn2(ps, pn_new(extra, 1));
            }
        }

        pa_set_ps(pa, ps);

        if (op == OP_UNLINK) {
            ps_set_unlinked(ps);
        } else if (op == OP_MKDIR) {
            ps_set_dir(ps);
        } else if (op == OP_SYMLINK) {
            ps_set_symlinked(ps);
        } else if (op == OP_LINK) {
            ps_set_linked(ps);
        }

        if (!pa_is_read(pa)) {
            moment_s tstamp;

            // Read ops don't need to be time-stamped.
            moment_get_systime(&tstamp);
            pa_set_timestamp(pa, tstamp);
        }

        // Finish by recording this path action.
        _thread_mutex_lock();
        ca_record_pa(CurrentCA, pa);
        _thread_mutex_unlock();
    }
}

// NOTE: We currently open and close two sockets per audited cmd;
// a 1st to deliver the SOA at initialization time, after which we
// block until it's closed by the monitor, and then a 2nd socket at
// finalization time to deliver everything else. I've considered
// trying to keep a single socket open right through on the theory
// (untested) that opening two sockets has a measurable cost, but
// so far am holding back for a few reasons: (1) it's not been
// shown to matter, (2) by holding sockets open from cmd start to
// end we increase the risk of running out of file descriptors
// in the monitor. This may only become an issue in deeply nested
// process trees, such as a deep recursive make, but it could happen.
// Also, (3) there are some buffering and ordering issues I don't
// understand well enough. These can be very subtle. The rule is that
// SOA absolutely MUST be seen before (a) its matching EOA and
// (b) any audit data from its children. Since the monitor loops
// through file descriptors in order (sockmin=>sockmax), if we hold
// some sockets open then is it possible a child might get issued
// a lower-numbered descriptor than its parent and thus be seen
// first? I haven't thought, or tested, it out. However, the auditor
// code is designed to be easily converted to use a single socket
// in the event the change is someday made.
static void
_monitor_open(SOCKET *sockp, CCS call)
{
    int so_reuse = 1;
    struct sockaddr_in dest_addr;
    CCS host;
    CCS portstr;
    char *nextport;
    u_short port;
    char host_port[256] = "???";
    int tries;

    vb_printf(VB_MON, "OPENING SOCKET FROM %s", call);

    if (*sockp != INVALID_SOCKET) {
        vb_printf(VB_MON, "ALREADY OPEN: SOCKET %d", *sockp);
        return;
    }

    TRY((*sockp = socket(PF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET, "socket()");

    TRY(setsockopt(*sockp, SOL_SOCKET, SO_REUSEADDR,
                    &so_reuse, sizeof(so_reuse)) != SOCKET_ERROR, "setsockopt(SO_REUSEADDR)");

    TRY(setsockopt(*sockp, SOL_SOCKET, SO_REUSEPORT, &so_reuse, sizeof(so_reuse)) != SOCKET_ERROR,
                    "setsockopt(SO_REUSEPORT)");

    host = prop_get_str(P_MONITOR_HOST);
    portstr = prop_get_str(P_MONITOR_PORT);

    for (tries = 0; portstr && *portstr; tries++) {
        port = (u_short)strtoul(portstr, &nextport, 0);
        portstr = nextport + 1;
        snprintf(host_port, charlen(host_port), "%s:%u", host, port);

        if (tries) {
            vb_printf(VB_MON, "Retrying on %s in %lu",
                host_port, (unsigned long)getpid());
        }

        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = PF_INET;
        dest_addr.sin_addr.s_addr = inet_addr(host);
        dest_addr.sin_port = htons(port);

        if (!connect(*sockp, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))) {
            vb_printf(VB_MON, "OPENED: SOCKET %d", *sockp);
            return;
        }

        if (errno == ETIMEDOUT || errno == EINTR) {
            vb_printf(VB_STD, "RETRY CONNECT");
            continue;
        } else if (errno == EISCONN) {
            vb_printf(VB_MON, "CONNECTED: SOCKET %d", *sockp);
            return;
        }
    }

    putil_syserr(2, host_port);
    exit(2);
}

// Internal service routine.
static void
_monitor_send(SOCKET *sockp, const void *buf, size_t len)
{
    vb_printf(VB_MON, "SENDING TO: SOCKET %d", *sockp);
    TRY(util_send_all(*sockp, buf, len, 0) >= 0, "util_send_all()");
}

// Internal service routine.
static void
_monitor_recv_line(SOCKET *sockp, const void *vbuf, size_t len)
{
    char *buf, *p;
    ssize_t ret;

    vb_printf(VB_MON, "RECVING FROM: SOCKET %d", *sockp);
    buf = (char *)vbuf;
    (void)memset(buf, 0, len);
    for (p = buf;;) {
        ret = recv(*sockp, p, len - (p - buf), 0);
        if (ret == INVALID_SOCKET) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            putil_syserr(2, "recv()");
            exit(2);
            break;
        } else if (ret == 0) {
            break;
        } else {
            char *e;

            if ((e = strchr(p, '\n'))) {
                *e = '\0';
                break;
            } else {
                p += ret;
            }
        }
    }
}

// Internal service routine.
static void
_monitor_flush(SOCKET *sockp)
{
    vb_printf(VB_MON, "FLUSHING: SOCKET %d", *sockp);
    (void)shutdown(*sockp, 1);
}

// Internal service routine.
static void
_monitor_close(SOCKET *sockp, int exiting)
{
    vb_printf(VB_MON, "CLOSING: SOCKET %d [exiting=%d]", *sockp, exiting);
    if (*sockp != INVALID_SOCKET) {
        close(*sockp);
        *sockp = INVALID_SOCKET;
    }
}

// Internal service routine. Return a file descriptor suitable
// for writing the audit record into.
static int
_audit_open(void)
{
    int fd;

    if (!_auditor_isActive()) {
        return -1;
    }

    // For debugging purposes we allow a mode where output is written
    // directly to a file descriptor. In the normal case it's sent to
    // a socket and received by the monitor.
    if (prop_is_true(P_NO_MONITOR)) {
        CCS ofile;

        if ((ofile = prop_get_str(P_OUTPUT_FILE))) {
            if (!strcmp(ofile, "-")) {
                fd = STDOUT_FILENO;
            } else if (!strcmp(ofile, "=")) {
                fd = STDERR_FILENO;
            } else {
                // If the file already exists, just append to it.
                // If not, make a new file with the pid appended.
                fd = open_real(ofile, O_WRONLY | O_APPEND, 0);
                if (fd == -1) {
                    char *obuf;

                    asprintf(&obuf, "%s.%lu", ofile, (unsigned long)getpid());
                    fd = open_real(obuf, O_CREAT | O_WRONLY | O_APPEND, 0666);
                    if (fd == -1) {
                        putil_syserr(2, obuf);
                        exit(2);
                    }
                    FREE(obuf);
                }
                fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
            }
        } else {
            fd = STDERR_FILENO;
        }
    } else {
        int i;

        FILE *tfp;

        // Create an unnamed/unlinked temp file. This returns a stream,
        // not a descriptor, so in order to avoid using up stdio streams
        // convert it to a high-numbered descriptor and close the stream.
        TRY(tfp = tmpfile(), "tmpfile()");

        // If our preferred fd is not in use, try using it.
        // Try a few above that next - this is in case of a
        // fork where we want the child side to have its own
        // temp file in order to keep the contents from being
        // scrambled.
        for (fd = -1, i = 0; i < 10; i++) {
            if (lseek(PREFERRED_FD + i, 0, SEEK_CUR) == -1) {
                if (dup2(fileno(tfp), PREFERRED_FD + i) != -1) {
                    fd = PREFERRED_FD + i;
                    break;
                }
            }
        }

        // If all the above efforts failed, just ask the system
        // for the next available descriptor.
        if (fd == -1) {
            if ((fd = dup(fileno(tfp))) == -1) {
                putil_syserr(2, "dup");
                exit(2);
            }
        }

        // Now the stream can go away to preserve limited stdio count.
        TRY(!fclose(tfp), "fclose()");

        // Each new cmd and/or process must create its own temp file.
        TRY(fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC) != -1, "fcntl()");
    }

    return fd;
}

// Internal service routine. Send an SOA record to the appropriate
// place (to monitor via socket or debug file).
static void
_audit_start(CCS call)
{
    CCS hdr;
    CS soa_hdr;

    if (!_auditor_isActive()) {
        return;
    }

    // Allocate and format the cmdline SOA header.
    hdr = ca_format_header(CurrentCA);
    if (!hdr || asprintf(&soa_hdr, "%s%s", SOA, hdr) < 0) {
        putil_syserr(2, NULL);
        exit(2);
    }
    FREE(hdr);

    // This is an optimization hack - lower-case is used to indicate
    // that this command is part of an aggregation and thus need not
    // be shopped for.
    if (prop_is_true(P_AGGREGATED_SUBCMD)) {
        soa_hdr[1] = 's';
    }

    // Acquire the descriptor we'll be using for audit reporting.
    AuditFD = _audit_open();

    // Write the SOA record into the file descriptor or socket.
    // Writes and sends are separated because you can't write()
    // to a socket on Windows and can't send() to a file anywhere.
    if (!prop_is_true(P_NO_MONITOR)) {
        char ack_soa[ACK_BUFFER_SIZE], *p;

        // The SOA must arrive at the monitor before the SOAs of any
        // children.  Thus we do not stash it for later delivery
        // but send it synchronously now. This is unfortunate
        // since two socket negotiations may be significantly more
        // expensive than one, though that has not been proven.
        // It's worth thinking this through again to see if there's
        // way around it, either by keeping the socket open or
        // allowing SOA to be sent in the same packet as EOA.
        _monitor_open(&ReportSocket, call);
        _monitor_send(&ReportSocket, soa_hdr, strlen(soa_hdr));
        FREE(soa_hdr);

        // Block until monitor acknowledges receipt of SOA.
        // This is needed to make sure it doesn't see EOA first.
        // We require the received message to be terminated with a newline;
        // we don't actually want the newline but it's a good way to
        // be sure we've read the whole message.
        _monitor_flush(&ReportSocket);
        _monitor_recv_line(&ReportSocket, ack_soa, sizeof(ack_soa));
        _monitor_close(&ReportSocket, 0);

        vb_printf(VB_MON, "CONTINUING [%s] WITH %s", ack_soa, ca_get_line(CurrentCA));

        if (!strcmp(ack_soa, ACK_FAILURE)) {
            _audit_end("exit", EXITING, 2);
            exit(2);
        } else if (!strcmp(ack_soa, ACK_OK_AGG)) {
            // Optimization: suppress shopping in aggregated children.
            if (prop_has_value(P_AGGREGATED_SUBCMD)) {
                prop_set_ulong(P_AGGREGATED_SUBCMD, 1);
            }
        } else if (!strcmp(ack_soa, ACK_OK)) {
            // Everything normal - carry on.
        } else {
            // We now know the command was successfully recycled so
            // we can quit and go home early.
            if ((p = strstr(ack_soa, FS1))) {
                *p = '\0';
                ca_set_recycled(CurrentCA, ack_soa);
            }

            // This should trigger an immediate EOA with a
            // field showing which PTX it was recycled from.
            // Exiting is VERY tricky here with all the variants
            // and hooks.
            // This seems to work but could maybe be done better.
            _audit_end("exit", EXITING, 0);
            exit(0);
        }
    } else {
        TRY(write(AuditFD, soa_hdr, strlen(soa_hdr)) != -1, "write()");
        FREE(soa_hdr);
    }

    return;
}

// Dump and flush whatever file data we've acquired. This is done
// just before the process disappears through either exit or exec.
// It's also done before fork, in which case it may be partial.
// We must also deal with path accesses collected on the
// child side of a fork before an exec, if any.
// Before any child process is created or this process
// is overlaid via exec we must make sure the current
// audit state is not lost (exec) or duplicated (fork).
// In addition, it's critical that the SOA be seen by the
// monitor BEFORE any audit data arrives from a subsidiary cmd.
// Thus the SOA must be sent ASAP but file activity data need
// only be collected for later transmission.
static void
_audit_flush(CCS call)
{
    UNUSED(call);

    if (!_auditor_isActive()) {
        return;
    }

    // Technically we may not need to synchronize here when called
    // during exit processing, but when called from anywhere else
    // we do, so the simplest implementation is to lock within this
    // function rather than at most of the calls to it.
    // This lock protects writes to both CurrentCA *and* AuditFD.
    _thread_mutex_lock();

    // This is an example of something we might want to do during
    // debugging if a host process is catching SIGSEGV (as Sun's
    // JVM does for instance). A case could be made that we could
    // reset *all* signal dispositions during finish processing
    // since the application thinks it's done.
    /* assert(signal(SIGSEGV, SIG_DFL) != SIG_ERR); */

    // Walk the tree, printing each pa node to the fd.
    // Note - ca_get_recycled is most likely redundant with ca_get_pa_count.
    if (!ca_get_recycled(CurrentCA) && ca_get_pa_count(CurrentCA)) {
        ca_write(CurrentCA, AuditFD);
    }

    // Let the lock go.
    _thread_mutex_unlock();
}

// This is called when a process is "ending" - either exit()
// or exec(). The difference is that you never know whether an
// exec will succeed.
static void
_audit_end(CCS call, int exiting, long status)
{
    int n;
    char ack_eoa[ACK_BUFFER_SIZE];

    if (!_auditor_isActive()) {
        return;
    }

    // This will start and/or flush the audit as required.
    _audit_flush(call);

#if defined(SIGPIPE) && defined(SIG_IGN)
    // This fixes a subtle problem: on a successful recycling event,
    // the monitor will cause the audited command to exit early. If
    // that program has a child which is writing to it through a pipe,
    // the effect of exiting the parent will be a SIGPIPE in the
    // child which will typically cause it to fail. Thus, when we're
    // about to finish up we surreptitiously force the audited
    // process to ignore SIGPIPE. Unlikely to break anything
    // since we're exiting anyway but yes it's an intrusive hack.
    // TODO - this placement seems overly aggressive because it will
    // ignore SIGPIPE on all exits and execs, whereas in theory
    // we should only need this when exiting early due to
    // recycle. But I tried moving it once and it broke so I moved
    // it back and added this note. Maybe I put it in the wrong
    // place. Analyze someday.
    // A downloading build of OpenSSL 1.0 demo-ed this problem
    // back in the day.
    {
        struct sigaction sigpipe = {0};

        sigpipe.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &sigpipe, NULL);
    }
#endif // defined(SIGPIPE) && defined(SIG_IGN)

    if (!prop_is_true(P_NO_MONITOR)) {
        if (AuditFD != -1 && CurrentCA && !ca_get_recycled(CurrentCA)) {
            char buf[1024];

            _monitor_open(&ReportSocket, call);

            // Rewind the temp file and send its contents to the monitor.
            TRY(lseek(AuditFD, 0, SEEK_SET) != (off_t)-1, "lseek()");

            while ((n = read(AuditFD, buf, sizeof(buf))) != 0) {
                if (n == -1) {
                    if (errno == EINTR) {
                        continue;
                    }
                    putil_syserr(2, "read");
                    exit(2);
                }
                _monitor_send(&ReportSocket, buf, n);
            }
        }
    }

    // Write the EOA marker if we're exiting AND an SOA was sent.
    if (exiting && CurrentCA && ca_get_started(CurrentCA)) {
        CCS hdr;
        CS eoa_hdr;

        // Allocate and format the cmdline header.
        hdr = ca_format_header(CurrentCA);
        assert(hdr);
        if (asprintf(&eoa_hdr, "%s[%ld]%s", EOA, status, hdr) < 0) {
            putil_syserr(2, NULL);
            exit(2);
        }
        FREE(hdr);

        if (ReportSocket == INVALID_SOCKET) {
            TRY(write(AuditFD, eoa_hdr, strlen(eoa_hdr)) != -1, "write()");
        } else {
            _monitor_send(&ReportSocket, eoa_hdr, strlen(eoa_hdr));
        }

        FREE(eoa_hdr);
        ca_destroy(CurrentCA);
        CurrentCA = NULL;
        close(AuditFD);
        AuditFD = -1;
    } else if (!prop_is_true(P_NO_MONITOR)) {
        // In case more data is written to the temp file, reposition
        // its pointer at the beginning.
        if (lseek(AuditFD, 0, SEEK_SET) == (off_t)-1) {
            putil_syserr(2, "lseek");
            exit(2);
        }
        TRY(!ftruncate(AuditFD, 0), "ftruncate()");
    }

    // No ACK needed in no-monitor mode ...
    if (ReportSocket == INVALID_SOCKET) {
        return;
    }

    // Block until monitor acknowledges receipt of EOA by closing
    // the other end.
    _monitor_flush(&ReportSocket);
    _monitor_recv_line(&ReportSocket, ack_eoa, sizeof(ack_eoa));

    // And of course close this end when done.
    _monitor_close(&ReportSocket, 1);
}

// Internal service routine.
// Note: it should be thread-safe to initialize static data here
// since this will be called at library initialization time when
// only one thread should be running.
static unsigned long
_init_auditlib(CCS call, CCS exe, CCS cmdstr)
{
    unsigned long pid;
    CCS rwd;
    CCS str;
    CCS pccode = NULL;

    // Stash the path to argv[0] asap because it could be modified.
    (void)putil_getexecpath();

    heap_init();

    // For a reason I don't understand, and would have to read
    // the Perl sources to learn, re-setting env vars with
    // putenv() causes perl to dump core though modifying them
    // in place is no problem.
    // Best guess is that perl maintains a static pointer to
    // the env block and the putenv() causes a realloc of said
    // env block leaving perl's pointer dangling.
    // The main point is that even if this isn't exactly what's
    // happening in perl such a case *could* happen in any
    // program. Therefore the rule is that the auditor must *NEVER*
    // add or modify an env var via putenv(). The monitor program must
    // make sure an EV is already present with sufficient padding
    // and the auditor can then twiddle those bits in place.
    // For instance, if an EV must hold a 32-bit value the monitor
    // must export it padded to 12 bytes which is the number
    // of chars needed to represent a null-terminated "-2147483648".
    // The initial value should come here like FOOBAR="          1".

    // Initialize verbosity asap.
    vb_init();

    if (prop_has_value(P_DEPTH)) {
        // Find the current depth and increment it for child cmds.
        // From here on P_DEPTH is a lie for the current cmd.
        Depth = prop_get_ulong(P_DEPTH);
        prop_set_ulong(P_DEPTH, Depth + 1);

        pccode = prop_get_str(P_PCCODE);
    } else {
        // Special case to allow "naked" testing of the auditor via e.g.
        // "LD_PRELOAD=/path/to/libAO.so some-command".
        prop_set_bool(P_NO_MONITOR, true);
        vb_printf(VB_OFF, "running in %s mode", P_NO_MONITOR);
    }

    // Initialize the hash-code generation.
    code_init();

    // We'll need this more than once, so remember it here.
    pid = getpid();

    // Optionally show commands as executed.
    if (vb_bitmatch(VB_EXEC)) {
        fprintf(vb_get_stream(), "+ %s\n", cmdstr);
    }

    // If full auditing is already activated, keep it going.
    // If not, and we have no activation RE, activate by default.
    // If we have an RE, apply it.
    if (!_auditor_isActive()) {
        void *re;

        if ((re = re_init_prop__(P_ACTIVATION_PROG_RE))) {
            if (re_match__(re, exe)) {
                vb_printf(VB_OFF, "ACTIVATED ON [%ld] '%s'", pid, exe);
                auditor_setActive();
                // Can't remove this EV - must change it in place.
                prop_set_str(P_ACTIVATION_PROG_RE, "");
            } else {
                vb_printf(VB_OFF, "INACTIVATED ON [%ld] '%s'", pid, exe);
                return pid;
            }
            re_fini__(&re);
        } else {
            vb_printf(VB_OFF, "ACTIVE ON [%ld] '%s'", pid, exe);
            _auditor_setActiveByDefault();
        }
    }

    // Set up a new audit object with starting values. The cmdid
    // is always identical to the pid just after an exec (here).
    // On Unix it will diverge on fork() and converge on the
    // next exec. This is why we need to manage the parent cmdid
    // in the environment rather than just trusting getppid().
    CurrentCA = ca_new();
    ca_set_cmdid(CurrentCA, pid);
    ca_set_depth(CurrentCA, Depth);
    ca_set_pcmdid(CurrentCA, prop_get_ulong(P_PCMDID));
    ca_set_prog(CurrentCA, exe);
    ca_set_rwd(CurrentCA, rwd = util_get_rwd());
    FREE(rwd);

    // This boolean means we've started a new cmd which will need
    // an SOA sent. Child processes should have this turned off,
    // because a process is NOT a cmd until an exec has taken place.
    ca_set_started(CurrentCA, 1);

    // Derive the current hostname.
    if (!ca_get_host(CurrentCA)) {
        char hostbuf[HOST_NAME_MAX];

        TRY(!gethostname(hostbuf, sizeof(hostbuf)), "gethostname()");
        ca_set_host(CurrentCA, hostbuf);
    }

    ca_set_line(CurrentCA, cmdstr);

    // If requested, and if the RE matches the current command,
    // record the current environment in the freetext field.
    if ((str = prop_get_str(P_TRACK_ENV_RE))) {
        void *cmd_re, *var_re;

        if (strrchr(str, ':')) {
            CS tstr, p;

            TRY(tstr = strdup(str), "strdup()");
            p = strrchr(tstr, ':');
            *p = '\0';
            cmd_re = re_init__("TRACK_ENV_CMD", tstr);
            var_re = re_init__("TRACK_ENV_VAR", p + 1);
            FREE(tstr);
        } else {
            cmd_re = re_init__("TRACK_ENV_CMD", str);
            var_re = NULL;
        }

        if (re_match__(cmd_re, cmdstr)) {
            CS eblock, encoded;
            size_t elen;
            char **envp;
            CS end;

            for (envp = environ, elen = 1; *envp; envp++) {
                elen += strlen(*envp) + 1;
            }

            TRY(eblock = malloc(elen), "malloc()");
            for (envp = environ, end = eblock; *envp && elen > 0; envp++) {
                if (!strstr(*envp, "TRACK_ENV")) {
                    if (!var_re || re_match__(var_re, *envp)) {
                        int n;

                        n = snprintf(end, elen, "%s\n", *envp);
                        end += n;
                        elen -= n;
                    }
                }
            }

            encoded = util_encode_minimal(eblock);
            FREE(eblock);
            ca_set_freetext(CurrentCA, encoded);
            FREE(encoded);
        }

        if (cmd_re) {
            re_fini__(&cmd_re);
        }
        if (var_re) {
            re_fini__(&var_re);
        }
    }

    // Retrieve the grandparent and parent cmd codes from the
    // environment and place them in the current object. Commands near
    // the top of the cmd tree may have no parent or grandparent.

    // Parent command ...
    if (!pccode || CSV_FIELD_IS_NULL(pccode)) {
        ca_set_pccode(CurrentCA, NULL);
    } else {
        char pccbuf[256];

        snprintf(pccbuf, charlen(pccbuf), "%s", pccode);
        ca_set_pccode(CurrentCA, util_strtrim(pccbuf));
    }

    _audit_start(call);

    // Push the current ccode into the environment as the
    // parent ccode. From here on the environment will be a lie
    // but we have the truth in the CA object.
    if (prop_has_value(P_PCCODE)) {
        prop_set_str(P_PCCODE, ca_get_ccode(CurrentCA));
    }

    // Special case - show the program itself as a file that we read
    // (because it is and we did).
    _pa_record(call, putil_getexecpath(), NULL, -1, OP_EXEC);

    // Potential instruction from the user to ignore certain files.
    IgnorePathRE = re_init_prop__(P_AUDIT_IGNORE_PATH_RE);

    // Potential instruction from the user to ignore certain programs.
    IgnoreProgRE = re_init_prop__(P_AUDIT_IGNORE_PROG_RE);

    return pid;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
