// Copyright (c) 2005-2010 David Boyce.  All rights reserved.

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
/// @brief Definitions for the PathAction "class".
/// A PathAction describes a moment in the life of a file when it was
/// acted upon in some way. It might simply record a case when the file
/// was read, or it could record a "creation event" such as a write,
/// rename, or unlink. A PathAction contains a PathState (see ps.c). For a
/// given PathState there may be many PathActions, though all but one (or
/// zero) of them must be read operations since a creation event would
/// typically result in a new PathState.
///
/// The term PathAction is often abbreviated as PA.

#include "AO.h"

#include "CA.h"
#include "PA.h"
#include "PS.h"

/// Object structure for describing audited files.
struct path_action_s {
    op_e pa_op;			//!< operation: read,append,link,etc
    CCS pa_call;		//!< name of accessing libc function
    moment_s pa_timestamp;	//!< time of write op
    unsigned long pa_pid;	//!< pid of accessing process
    unsigned long pa_ppid;	//!< parent pid of accessing process
    unsigned long pa_tid;	//!< thread id within accessing process
    unsigned long pa_depth;	//!< # of execs from start to here
    CCS pa_pccode;		//!< identity hash of parent CA
    CCS pa_ccode;		//!< identity hash of our CA
    int pa_fd;			//!< file descriptor if opened
    int pa_uploadable;		//!< is this file to be uploaded?
    ps_o pa_ps;			//!< intrinsic (stat etc) data
} path_action_s;

/// Ordering function (NOT method) for placing PAs in a data structure.
/// @param[in] left     the "left" object pointer
/// @param[in] right    the "right" object pointer
/// @return 1, 0, or -1 as left is greater than, equal to, or less than right
int
pa_cmp_by_pathname(const void *left, const void *right)
{
    int rc;

    if (left == right) {
	rc = 0;
    } else {
	pa_o pa1, pa2;
	CCS path1, path2;

	pa1 = (pa_o)left;
	pa2 = (pa_o)right;

	// Order by relative path rather than absolute.
	// This will only affect members and will cause them
	// to cluster separately which may improve readability.
	path1 = pa_get_rel(pa1);
	path2 = pa_get_rel(pa2);

	rc = util_pathcmp(path1, path2);
    }

    return rc;
}

/// Ordering function (NOT method) for placing PAs in a data structure.
/// This function needs to do double duty for both equality and
/// ordering. We interpret equality as actual object identity, meaning
/// the pointers are the same. If the pointers are not the same,
/// we must *not* return 0 and there it becomes a matter of the
/// preferred ordering. The logic below puts read ops ahead of write ops
/// simply because it's intuitive that you read inputs before writing
/// outputs. Within read ops we order by pathname, again for readability,
/// and as a tiebreaker we subtract pointers (which is a meaningless but
/// stable ordering). Within write ops we order by when the write occurred
/// which is crucial in the event of destructive ops like unlinks and
/// renames. In the rare event that two write ops happened at the exact
/// same instant we use the same tiebreaker as above.
/// The only critical ordering is that of timestamps on write ops; the
/// rest is for convenience. And note that "timestamps on write ops"
/// refers to the time that the op occurred, not the timestamp on the file.
/// The ordering does not depend at all on path state (size/mtime/dcode).
/// @param[in] left     the "left" object pointer
/// @param[in] right    the "right" object pointer
/// @return 1, 0, or -1 as left is greater than, equal to, or less than right
int
pa_cmp(const void *left, const void *right)
{
    int rc;

    if (left == right) {
	rc = 0;
    } else {
	pa_o pa1, pa2;

	pa1 = (pa_o)left;
	pa2 = (pa_o)right;

	if (pa_is_read(pa1)) {
	    if (pa_is_read(pa2)) {
		// Order by relative path rather than absolute.
		// This will only affect members and will cause them
		// to cluster separately which may improve readability.
		rc = util_pathcmp(pa_get_rel(pa1), pa_get_rel(pa2));
	    } else {
		rc = 1;
	    }
	} else if (pa_is_read(pa2)) {
	    rc = -1;
	} else {
	    // If neither is a read op, then they must both be write ops
	    // which implies that timestamps are available.
	    rc = moment_cmp(pa1->pa_timestamp, pa2->pa_timestamp, NULL);
	    if (rc == 0) {
		// If timestamps are identical, this may be a rename
		// which was recorded as an unlink and a create
		// happening at the same time. Naturally we want the
		// unlink to precede the create.
		if (pa_is_unlink(pa1)) {
		    rc = 1;
		} else if (pa_is_unlink(pa2)) {
		    rc = -1;
		}
	    }
	}

	// As a last resort, compare the pointers.
	if (rc == 0) {
	    rc = pa1 > pa2 ? 1 : -1;
	}
    }

    return rc;
}

/// Constructor.
/// @return a new PathAction object
pa_o
pa_new(void)
{
    pa_o pa;

    pa = (pa_o)putil_calloc(1, sizeof(*pa));

    return pa;
}

/// Converts a stringified path action back to a PA object.
/// Any change to the CSV format require an equivalent change here.
/// @param[in] csv      a stringified path action
/// @return a new PathAction object
pa_o
pa_newFromCSVString(CCS csv)
{
    CS linebuf, original;
    CCS op, call, timestamp, pid, depth, ppid, tid, pccode, ccode;
    pa_o pa;
    ps_o ps;

    // It would be elegant to use sscanf to parse this against the same
    // printf specification used to format it but unfortunately in scanf
    // a "%s" says to scan to the next whitespace character. Since we have
    // multiple strings in the format, any of which may contain whitespace,
    // scanf won't work.

    // The line buffer will be trashed by util_strsep(), so we
    // allocate a temporary copy. The pointer passed in will also
    // be changed, so we need to hold a copy of the original address
    // to be freed.
    linebuf = original = putil_strdup(csv);

    // *INDENT-OFF*
    if (    !(op         = util_strsep(&linebuf, FS1)) ||
	    !(call       = util_strsep(&linebuf, FS1)) ||
	    !(timestamp  = util_strsep(&linebuf, FS1)) ||
	    !(pid        = util_strsep(&linebuf, FS1)) ||
	    !(depth      = util_strsep(&linebuf, FS1)) ||
	    !(ppid       = util_strsep(&linebuf, FS1)) ||
	    !(tid        = util_strsep(&linebuf, FS1)) ||
	    !(pccode     = util_strsep(&linebuf, FS1)) ||
	    !(ccode      = util_strsep(&linebuf, FS1))) {
	putil_int("bad format: '%s'", csv);
	putil_free(original);
	return NULL;
    }
    // *INDENT-ON*

    pa = pa_new();
    pa_set_op(pa, (op_e)op[0]);	// op is just one char
    pa_set_call(pa, call);
    if (pa_set_timestamp_str(pa, timestamp)) {
	putil_int("bad format: %s", timestamp);
	putil_free(original);
	return NULL;
    }
    pa_set_pid(pa, strtoul(pid, NULL, 10));
    pa_set_depth(pa, strtoul(depth, NULL, 10));
    pa_set_ppid(pa, strtoul(ppid, NULL, 10));
    pa_set_tid(pa, strtoul(tid, NULL, 10));
    pa_set_pccode(pa, pccode);
    pa_set_ccode(pa, ccode);

    // Hand the remainder off to the PS class ...
    if (!(ps = ps_newFromCSVString(linebuf))) {
	putil_free(original);
	return NULL;
    }
    pa_set_ps(pa, ps);
    putil_free(original);
    return pa;
}

/// Format a PA for server consumption.
/// @param[in] pa       the object pointer
/// @param[out] buf     a buffer to hold the formatted string
/// @param[in] bufmax   the size of the passed-in buffer
/// @return the length of the formatted string
int
pa_toCSVString(pa_o pa, CS buf, int bufmax)
{
    int len;
    char timestamp[MOMENT_BUFMAX];

    (void)moment_format(pa->pa_timestamp, timestamp, charlen(timestamp));

    // *INDENT-OFF*
    len = snprintf(buf, bufmax,
		"%c%s"		// 1  - OP
		"%s%s"		// 2  - CALL
		"%s%s"		// 3  - TIMESTAMP
		"%lu%s"		// 4  - PID
		"%lu%s"		// 5  - DEPTH
		"%lu%s"		// 6  - PPID
		"%lu%s"		// 7  - TID
		"%s%s"		// 8  - PCCODE
		"%s%s",		// 9  - CCODE
	pa->pa_op, FS1,
	pa->pa_call, FS1,
	timestamp, FS1,
	pa->pa_pid, FS1,
	pa->pa_depth, FS1,
	pa->pa_ppid, FS1,
	pa->pa_tid, FS1,
	pa_get_pccode(pa), FS1,
	pa_get_ccode(pa), FS1);
    // *INDENT-ON*

    if (len < 0 || len > bufmax) {
	buf[bufmax - 1] = '\0';
	putil_syserr(0, buf);
    } else {
	int pslen;

	pslen = ps_toCSVString(pa->pa_ps, buf + len, bufmax - len);
	if (pslen < 0 || pslen >= bufmax) {
	    buf[bufmax - 1] = '\0';
	    putil_syserr(0, buf);
	} else {
	    strcpy(buf + len + pslen, "\n");
	}
    }

    return strlen(buf);
}

/// Format a PathAction for user consumption (typically debugging)
/// @param[in] pa       the object pointer
/// @return an allocated string which must be freed by the caller
CCS
pa_tostring(pa_o pa)
{
    char line[(PATH_MAX * 2) + 256];

    if (pa_exists(pa)) {
	line[0] = '\0';
    } else {
	strcpy(line, "(GONE) ");
    }

    (void)pa_toCSVString(pa, endof(line), leftlen(line));

    return putil_strdup(line);
}

/// Boolean - returns true iff the path is a member of the project.
/// @param[in] pa       the object pointer
/// @return true or false
int
pa_is_member(pa_o pa)
{
    return ps_is_member(pa->pa_ps);
}

/// Delegates to ps_is_dir().
/// @param[in] pa       the object pointer
/// @return true or false
int
pa_is_dir(pa_o pa)
{
    return ps_is_dir(pa->pa_ps);
}

/// Delegates to ps_is_special().
/// @param[in] pa       the object pointer
/// @return true or false
int
pa_is_special(pa_o pa)
{
    return ps_is_special(pa->pa_ps);
}

/// Boolean - returns true iff the operation was a read.
/// @param[in] pa       the object pointer
/// @return true or false
int
pa_is_read(pa_o pa)
{
    return (pa->pa_op == OP_EXEC) || (pa->pa_op == OP_READ);
}

/// Boolean - returns true iff the operation was a write.
/// @param[in] pa       the object pointer
/// @return true or false
int
pa_is_write(pa_o pa)
{
    return (pa->pa_op == OP_CREAT) || (pa->pa_op == OP_APPEND);
}

/// Boolean - returns true iff the operation was a hard link.
/// @param[in] pa       the object pointer
/// @return true or false
int
pa_is_link(pa_o pa)
{
    return pa->pa_op == OP_LINK;
}

/// Boolean - returns true iff the operation was a symbolic link.
/// @param[in] pa       the object pointer
/// @return true or false
int
pa_is_symlink(pa_o pa)
{
    return pa->pa_op == OP_SYMLINK;
}

/// Boolean - returns true iff the operation was a removal.
/// @param[in] pa       the object pointer
/// @return true or false
int
pa_is_unlink(pa_o pa)
{
    return pa->pa_op == OP_UNLINK;
}

/// Delegates to ps_exists().
int
pa_exists(pa_o pa)
{
    return ps_exists(pa->pa_ps);
}

/// Delegates to ps_set_moment().
void
pa_set_moment(pa_o pa, moment_s moment)
{
    ps_set_moment(pa->pa_ps, moment);
}

/// Delegates to ps_set_moment_str().
int
pa_set_moment_str(pa_o pa, CCS str)
{
    return ps_set_moment_str(pa->pa_ps, str);
}

/// Sets the timestamp field by parsing a stringified form.
/// @param[in] pa       the object pointer
/// @param[in] str      a stringified timestamp in the moment_s format
/// @return 0 on success
int
pa_set_timestamp_str(pa_o pa, CCS str)
{
    return moment_parse(&(pa->pa_timestamp), str);
}

/// Queries whether the current PA has a timestamp already.
/// @param[in] pa       the object pointer
/// @return nonzero iff a timestamp exists
int
pa_has_timestamp(pa_o pa)
{
    return moment_is_set(pa->pa_timestamp);
}

/// Delegates to ps_set_size_str().
void
pa_set_size_str(pa_o pa, CCS str)
{
    ps_set_size_str(pa->pa_ps, str);
}

/// Delegates to ps_set_dcode().
void
pa_set_dcode(pa_o pa, CCS dcode)
{
    ps_set_dcode(pa->pa_ps, dcode);
}

/// Delegates to ps_has_dcode().
int
pa_has_dcode(pa_o pa)
{
    return ps_has_dcode(pa_get_ps(pa));
}

/// Delegates to ps_stat().
/// @param[in] pa               the object pointer
/// @param[in] want_dcode       boolean - derive dcode iff true
/// @return 0 on success
int
pa_stat(pa_o pa, int want_dcode)
{
    return ps_stat(pa->pa_ps, want_dcode);
}

/// Delegates to ps_diff().
/// @param[in] pa1      the left object pointer
/// @param[in] pa2      the right object pointer
/// @return a static string describing the difference, or NULL if the same
CCS
pa_diff(pa_o pa1, pa_o pa2)
{
    return ps_diff(pa1->pa_ps, pa2->pa_ps);
}

/// Makes a deep copy of an existing PA with a potentially different name.
/// Warning: use with care to avoid memory leaks.
/// @param[in] cpa      the PA object pointer to be copied
/// @return the new PA object
pa_o
pa_copy(pa_o cpa)
{
    pa_o npa;
    ps_o nps;

    npa = pa_new();

    pa_set_op(npa, pa_get_op(cpa));
    pa_set_call(npa, pa_get_call(cpa));
    pa_set_timestamp(npa, pa_get_timestamp(cpa));
    pa_set_pid(npa, pa_get_pid(cpa));
    pa_set_ppid(npa, pa_get_ppid(cpa));
    pa_set_tid(npa, pa_get_tid(cpa));
    pa_set_depth(npa, pa_get_depth(cpa));
    pa_set_pccode(npa, pa_get_pccode(cpa));
    pa_set_ccode(npa, pa_get_ccode(cpa));
    pa_set_fd(npa, pa_get_fd(cpa));
    pa_set_uploadable(npa, pa_get_uploadable(cpa));

    nps = ps_copy(pa_get_ps(cpa));
    pa_set_ps(npa, nps);

    return npa;
}

// *INDENT-OFF*
// Generally the 'call' is supplied as a literal string (e.g. "open")
// so it could be used as is. But for consistency and to avoid
// subtle bugs we malloc and free it like other values.
GEN_SETTER_GETTER_DEFN_STR(pa, call, CCS, NULL)

GEN_SETTER_GETTER_DEFN(pa,	op,		op_e)
GEN_SETTER_GETTER_DEFN(pa,	timestamp,	moment_s)
GEN_SETTER_GETTER_DEFN(pa,	pid,		unsigned long)
GEN_SETTER_GETTER_DEFN(pa,	ppid,		unsigned long)
GEN_SETTER_GETTER_DEFN(pa,	tid,		unsigned long)
GEN_SETTER_GETTER_DEFN(pa,	depth,		unsigned long)
GEN_SETTER_GETTER_DEFN_STR(pa,	pccode,		CCS, CSV_NULL_FIELD)
GEN_SETTER_GETTER_DEFN_STR(pa,	ccode,		CCS, CSV_NULL_FIELD)
GEN_SETTER_GETTER_DEFN(pa,	fd,		int)
GEN_SETTER_GETTER_DEFN(pa,	uploadable,	int)
GEN_SETTER_GETTER_DEFN(pa,	ps,		ps_o)

GEN_DELEGATE_GETTER_DEFN(pa, ps, pn, pn_o)
GEN_DELEGATE_GETTER_DEFN(pa, ps, abs, CCS)
GEN_DELEGATE_GETTER_DEFN(pa, ps, rel, CCS)
GEN_DELEGATE_GETTER_DEFN(pa, ps, abs2, CCS)
GEN_DELEGATE_GETTER_DEFN(pa, ps, rel2, CCS)
GEN_DELEGATE_GETTER_DEFN(pa, ps, target, CCS)
GEN_DELEGATE_GETTER_DEFN(pa, ps, dcode, CCS)
GEN_DELEGATE_GETTER_DEFN(pa, ps, datatype, int)
GEN_DELEGATE_GETTER_DEFN(pa, ps, size, int64_t)
GEN_DELEGATE_GETTER_DEFN(pa, ps, moment, moment_s)
// *INDENT-ON*

/// Finalizer - releases all allocated memory for object.
/// If object is a null pointer, no action occurs.
/// @param[in] pa       the object pointer
void
pa_destroy(pa_o pa)
{
    if (!pa) {
	return;
    }
    // Free the contained ps object.
    ps_destroy(pa->pa_ps);

    // Free all string pointers.
    putil_free(pa->pa_call);
    putil_free(pa->pa_pccode);
    putil_free(pa->pa_ccode);

    // Zero-fill the struct before freeing it - nicer for debugging.
    memset(pa, 0, sizeof(*pa));

    // Last, free the object itself.
    putil_free(pa);
}
