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
/// @brief Definitions for the PathState "class".
/// A PathState represents the "state" of a file over a certain period of
/// time, in terms of file size, datestamp, contents, and lesser metadata
/// such as owner, group, and permissions.  A PathState contains a PathName
/// (see pn.c). The underlying path state is in existence for some quantum
/// of time and may be opened any number of times during that period. Once
/// the file is written to, or renamed or unlinked, the previous state is
/// no longer in existence but the PathState object representing it may
/// live on in the historical record like a fossil.
///
/// The term PathState is often abbreviated as PS.

#include "AO.h"

#include "CODE.h"
#include "PN.h"
#include "PROP.h"
#include "PS.h"

#include "hash.h"

#include "curl/curl.h"

/// @cond static
#define	PS_NO_DCODE			_T("")
/// @endcond static

/// Related to but subtly different from PA ops. These describe a
/// <I>state</I> whereas PA ops describe an <I>action</I>.
typedef enum {
    PS_FILE = 'f',		/// Path represents a file
    PS_DIR  = 'd',		/// Path represents a directory
    PS_LINK = 'l',		/// Path has been linked to
    PS_SYMLINK = 's',		/// Path represents a symlink
    PS_UNLINK = 'u',		/// Path has been unlinked
} ps_e;

/// Object structure for describing the state of a file at a given time.
// NOTE: we use int64_t for file sizes in preference to off_t because
// Windows forces off_t to 'long' and we need to be sure we can
// represent 64-bit sizes.
struct path_state_s {
    moment_s ps_moment;		///< mtime as secs+nanos
    int64_t ps_size;		///< file size
    mode_t ps_mode;		///< file mode (perms + type)
    CCS ps_dcode;		///< "data code" as of sample time
    CCS ps_fsname;		///< name of path's file system
    pn_o ps_pn;			///< path to file as of sample time
    pn_o ps_pn2;		///< used for some link ops
    CCS ps_target;		///< target text, if symlink
    ps_e ps_datatype;		///< file type: 'f', 's', etc
} path_state_s;

static moment_s Ref_Time;

static long Dcode_Offset = -1;

static hash_t *Dcode_Hash_Table;

/// Constructor.
/// @return a new PathState object
ps_o
ps_new(void)
{
    ps_o ps;

    ps = (ps_o)putil_calloc(1, sizeof(*ps));
    ps->ps_datatype = PS_FILE;	// default
    return ps;
}

/// Constructor.
/// @param[in] path     a string representing the path
/// @return a new PathState object
ps_o
ps_newFromPath(CCS path)
{
    ps_o ps;

    ps = ps_new();
    ps_set_pn(ps, pn_new(path, 1));
    return ps;
}

/// Converts a stringified path state back to a PS object.
/// See pa_newFromCSVString().
/// @param[in] csv      a stringified path state
/// @return a new PathState object
ps_o
ps_newFromCSVString(CCS csv)
{
    CS linebuf, original;
    CCS datatype, fsname, moment, size, mode, dcode, target;
    ps_o ps;

    // The line buffer will be trashed by util_strsep(), so we
    // allocate a temporary copy. The pointer passed in will also
    // be changed, so we need to hold a copy of the original address
    // to be freed.
    linebuf = original = putil_strdup(csv);

    // *INDENT-OFF*
    if (!(datatype = util_strsep(&linebuf, FS1)) ||
	    !(fsname = util_strsep(&linebuf, FS1)) ||
	    !(moment = util_strsep(&linebuf, FS1)) ||
	    !(size = util_strsep(&linebuf, FS1)) ||
	    !(mode = util_strsep(&linebuf, FS1)) ||
	    !(dcode = util_strsep(&linebuf, FS1)) ||
	    !(target = util_strsep(&linebuf, FS1))) {
	putil_int(_T("bad format: '%s'"), csv);
	putil_free(original);
	return NULL;
    }
    // *INDENT-ON*

    ps = ps_new();

    ps_set_datatype(ps, *datatype);
    ps_set_fsname(ps, fsname && *fsname != '?' ? fsname : NULL);
    ps_set_moment_str(ps, moment);
    ps_set_size(ps, _tcstoul(size, NULL, 10));
    ps_set_mode(ps, _tcstoul(mode, NULL, CSV_RADIX));
    ps_set_dcode(ps, dcode);

    // The link target is URL-encoded in CSV format.
    if (target && *target) {
	CS decoded;

	decoded = util_unescape(target, 0, NULL);
	if (*datatype == PS_SYMLINK) {
	    ps_set_target(ps, decoded);
	} else {
	    ps_set_pn2(ps, pn_new(decoded, 0));
	}
	putil_free(decoded);
    }

    ps_set_pn(ps, pn_new(linebuf, 0));

    putil_free(original);
    return ps;
}

/// Boolean - returns true iff the path is a member of the project.
/// @param[in] ps       the object pointer
/// @return true or false
int
ps_is_member(ps_o ps)
{
    return pn_is_member(ps->ps_pn);
}

/// Boolean - returns true iff this object is a regular file.
/// @param[in] ps       the object pointer
/// @return true or false
int
ps_is_file(ps_o ps)
{
    return S_ISREG(ps->ps_mode);
}

/// Boolean - returns true iff this object is a directory.
/// @param[in] ps       the object pointer
/// @return true or false
int
ps_is_dir(ps_o ps)
{
    return S_ISDIR(ps->ps_mode);
}

/// Boolean - returns true iff this object is some kind of "special" file.
/// @param[in] ps       the object pointer
/// @return true or false
int
ps_is_special(ps_o ps)
{
#if defined(_WIN32)
    UNUSED(ps);
    return 0;
#else	/*_WIN32*/
    return S_ISBLK(ps->ps_mode) || S_ISCHR(ps->ps_mode)
	|| S_ISFIFO(ps->ps_mode) || S_ISSOCK(ps->ps_mode);
#endif	/*_WIN32*/
}

/// Boolean - returns true iff this object is the result of a hard link op.
/// @param[in] ps       the object pointer
/// @return true or false
int
ps_is_link(ps_o ps)
{
    return ps->ps_datatype == PS_LINK;
}

/// Boolean - returns true iff this object represents a symbolic link.
/// @param[in] ps       the object pointer
/// @return true or false
int
ps_is_symlink(ps_o ps)
{
    return ps->ps_datatype == PS_SYMLINK;
}

/// Boolean - returns true iff this object is the result of an unlink op
/// (and thus should not exist).
/// @param[in] ps       the object pointer
/// @return true or false
int
ps_is_unlink(ps_o ps)
{
    return ps->ps_datatype == PS_UNLINK;
}

/// Boolean - returns true iff the path is currently present on this system.
/// @param[in] ps       the object pointer
/// @return true or false
int
ps_exists(ps_o ps)
{
    return pn_exists(ps->ps_pn);
}

// The following suite of functions enables "dcode caching". The idea
// is that if there's some service program which is large and thus slow
// to dcode (such as cc1), it's madness to do so many times in a build
// when it's years old and clearly not changing. So the design is that
// if the file size and timestamp are exactly the same as when it was
// last dcoded AND its timestamp precedes the build's starting time
// by some reasonable amount, as given in P_DCODE_CACHE_SECS, then we
// allow its dcode to be cached and reused. The second restriction
// can be relaxed by setting P_DCODE_CACHE_SECS to 0, in which case
// all files are subject to dcode caching. If P_DCODE_CACHE_SECS is
// set to -1 (the default), no dcode caching is done at all.
// NOTE: this feature seems to work fine in small-scale testing
// but has not shown any major performance improvement yet. Need
// to try it in a larger/longer build. If it doesn't add value there,
// take it out.

/// Enable dcode cache management based on the P_DCODE_* property settings.
void
ps_dcode_cache_init(void)
{
    Dcode_Offset = prop_get_long(P_DCODE_CACHE_SECS);

    if (Dcode_Offset >= 0) {
	moment_get_systime(&Ref_Time);
	if (!(Dcode_Hash_Table = hash_create(HASHCOUNT_T_MAX,
					     util_pathcmp, NULL))) {
	    putil_syserr(2, _T("hash_create(Dcode_Hash_Table)"));
	}
    }
}

/// Finalize dcode cache management if in use.
void
ps_dcode_cache_fini(void)
{
    if (Dcode_Hash_Table) {
	hscan_t hscan;
	hnode_t *hnp;
	CCS key, val;

	hash_scan_begin(&hscan, Dcode_Hash_Table);
	for (hnp = hash_scan_next(&hscan); hnp; hnp = hash_scan_next(&hscan)) {
	    hash_scan_delete(Dcode_Hash_Table, hnp);
	    key = (CCS)hnode_getkey(hnp);
	    val = (CCS)hnode_get(hnp);
	    hnode_destroy(hnp);
	    putil_free(key);
	    putil_free(val);
	}
	hash_destroy(Dcode_Hash_Table);
	Dcode_Hash_Table = NULL;
    }
}

// Internal service routine.
static void
_ps_set_cached_dcode(ps_o ps, CCS path)
{
    CS key;

    if (Dcode_Hash_Table && (Dcode_Offset == 0 ||
			     (ps->ps_moment.ntv_sec <
			      (Ref_Time.ntv_sec - Dcode_Offset)))) {
	TCHAR mtime[MOMENT_BUFMAX];

	(void)moment_format(ps->ps_moment, mtime, charlen(mtime));

	if (asprintf(&key, _T("%") _T(PRIu64) _T("%s%s%s%s"),
			   ps->ps_size, FS1, mtime, FS1, path) < 0) {
	    putil_syserr(2, NULL);
	}
	hash_alloc_insert(Dcode_Hash_Table, key, putil_strdup(ps->ps_dcode));
    }
}

// Internal service routine.
static CCS
_ps_get_cached_dcode(ps_o ps, CCS path)
{
    TCHAR keybuf[PATH_MAX * 2];
    hnode_t *hnp;
    CCS dcode;

    if (Dcode_Hash_Table) {
	TCHAR mtime[MOMENT_BUFMAX];

	(void)moment_format(ps->ps_moment, mtime, charlen(mtime));

	_sntprintf(keybuf, charlen(keybuf), _T("%") _T(PRIu64) _T("%s%s%s%s"),
		   ps->ps_size, FS1, mtime, FS1, path);
	if ((hnp = hash_lookup(Dcode_Hash_Table, keybuf))) {
	    dcode = (CCS)hnode_get(hnp);
	    vb_printf(VB_OFF, "USING CACHED DCODE FOR %s", path);
	    return dcode;
	}
    }

    return NULL;
}

/*
 * [Is this still true?]
 * Current linux ext3 implementations have a time stamp resolution bug:
 * in-core time resolution is 64 bits, but on-disk resolution is 32 bits
 *
 * the effect is that once file status info is flushed its stat() times
 * will be truncated to seconds
 *
 * this wreaks havoc on programs that record 64 bit timestamps for
 * future reference
 *
 * -- Glenn Fowler -- AT&T Research, Florham Park NJ --
 */
/// Sets the modification time from the supplied stat structure.
/// @param[in] path     the path to the file being sampled
/// @param[out] mptr    a pointer to a moment_s to be filled in
/// @param[in] stp      a pointer to a valid 'struct stat64'
/// @return 0 on success
static int
_ps_set_modtime(CCS path, moment_s *mptr, struct __stat64 *stp)
{
#if defined(_WIN32)
    HANDLE hFile;

    FILETIME ft;

    LARGE_INTEGER li;

    __int64 clunks;

    hFile = CreateFile(path,
		       GENERIC_READ,
		       FILE_SHARE_READ,
		       NULL,
		       OPEN_EXISTING,
		       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
		       NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
	return 1;
    }
    if (GetFileTime(hFile, NULL, NULL, &ft) == 0) {
	CloseHandle(hFile);
	return 1;
    }
    if (!CloseHandle(hFile)) {
	putil_win32err(0, GetLastError(), path);
    }

    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    clunks = li.QuadPart;	// In 100-nanosecond intervals
    clunks = ((LONGLONG) ft.dwHighDateTime) << 32;
    clunks += ft.dwLowDateTime - WIN_EPOCH_OFFSET;
    mptr->ntv_sec = (int64_t)(clunks / WIN_CLUNKS_PER_SEC);
    mptr->ntv_nsec = (long)((clunks % WIN_CLUNKS_PER_SEC) * 100);
#else	/*_WIN32*/

    UNUSED(path);

// Funny little hack-on-hack: many modern Unices hack
// sub-second resolution into the stat structure and then
// hide it (for backward compatibility) by defining st_mtime
// to be a pointer to the seconds part. Therefore, we assume
// that if st_mtime is defined as a macro then sub-second
// resolution must be available.
#if defined(st_mtime)
    mptr->ntv_sec = stp->st_mtim.tv_sec;
    mptr->ntv_nsec = stp->st_mtim.tv_nsec;
#else	/*st_mtime*/
    mptr->ntv_sec = stp->st_mtime;
    mptr->ntv_nsec = 0;
#endif	/*st_mtime*/

#endif	/*_WIN32*/

    return 0;
}

/// Samples the contained pathname and stores its vital statistics.
/// @param[in] ps               the object pointer
/// @param[in] want_dcode       boolean - derive dcode iff true
/// @return 0 on success
int
ps_stat(ps_o ps, int want_dcode)
{
    CCS path;

    int statrc;

    struct __stat64 stbuf;

    path = ps_get_abs(ps);	// convenience

    // Delay return on a failure here till the fsname is determined.
    statrc = _tlstat64(path, &stbuf);

    // No need to derive the fs name twice. File contents and metadata
    // can change, but we assume the name of the filesystem within which
    // it resides will not change during a build.
    // fsstat() appears to be painfully slow on Linux, or maybe
    // it's ext3. Anyway, for now we don't really need the fsname value.
    // If we do, we may need to cache it (per path or per parent dir)
    // or something similar. Actually on further investigation it may not
    // be that bad after all, so if ever needed we should start by just
    // enabling it and measuring the effect.
    if (0 && !ps_get_fsname(ps)) {
	TCHAR fs[PATH_MAX + 1];

	ino_t midway;

	// Stat the file system containing this file to get its name.
	// We don't abort even on failure; it will always give us a string.
	// We can derive this datum even for files which don't exist
	// by falling back to the parent directory, recursively.
	(void)util_find_fsname(path, fs, charlen(fs));

	// Special case for ClearCase MVFS - view-private files
	// have a negative inode, versioned elements are positive.
	// However, since ino_t is unsigned the view-privates
	// show up as large positive numbers.
	// Record the distinction because it may be useful.
	midway = ((ino_t) - 1) / 2;
	if (!statrc && stbuf.st_ino > midway && !_tcscmp(fs, _T("mvfs"))) {
	    ps_set_fsname(ps, _T("mvfs-vp"));
	} else {
	    ps_set_fsname(ps, fs);
	}
    }

    if (statrc) {
	return -1;
    }

    ps->ps_size = stbuf.st_size;
    ps->ps_mode = stbuf.st_mode;

    if (S_ISDIR(stbuf.st_mode)) {
	ps_set_dir(ps);
    }

    if (_ps_set_modtime(path, &(ps->ps_moment), &stbuf)) {
	putil_syserr(0, path);
	return -1;
    }
#if !defined(_WIN32)
    if (S_ISLNK(stbuf.st_mode)) {
	ps_set_symlinked(ps);
	if (!ps_get_target(ps)) {
	    int len;

	    char buf[PATH_MAX];

	    if ((len = readlink(path, buf, sizeof(buf))) == -1) {
		putil_syserr(0, path);
		return -1;
	    } else {
		buf[len] = '\0';
		ps_set_target(ps, buf);
	    }
	}
    }
#endif	/*_WIN32*/

    if (want_dcode) {
	CCS dcode;

	TCHAR dcbuf[CODE_IDENTITY_HASH_MAX_LEN];

	if (ps_is_file(ps)) {
	    if ((dcode = _ps_get_cached_dcode(ps, path))) {
		ps_set_dcode(ps, dcode);
	    } else if ((dcode = code_from_path(path, dcbuf, sizeof(dcbuf)))) {
		ps_set_dcode(ps, dcode);
		_ps_set_cached_dcode(ps, path);
	    } else {
		ps_set_dcode(ps, NULL);
		return -1;
	    }
	} else if (ps_is_symlink(ps)) {
	    CCS tgt;

	    // For symlinks, we use the target as the "file contents".
	    tgt = ps_get_target(ps);
	    if ((dcode = code_from_buffer((const unsigned char *)tgt,
					  _tcslen(tgt), path, dcbuf,
					  sizeof(dcbuf)))) {
		ps_set_dcode(ps, dcode);
		_ps_set_cached_dcode(ps, path);
	    } else {
		ps_set_dcode(ps, NULL);
		return -1;
	    }
	}
    }

    return 0;
}

/// Boolean - returns true iff the object has a valid dcode.
/// @param[in] ps               the object pointer
/// @return 1 iff a valid dcode is present
int
ps_has_dcode(ps_o ps)
{
    return ps->ps_dcode != NULL;
}

/// Compares two PathState objects. PathState objects are considered
/// identical if they have the same path, size, type, and dcode. If
/// no dcode is present, the timestamp is used instead.
/// @param[in] ps1      the left object pointer
/// @param[in] ps2      the right object pointer
/// @return a static string describing the difference, or NULL if the same
CCS
ps_diff(ps_o ps1, ps_o ps2)
{
    CCS reason = NULL;

    if (ps1->ps_datatype != ps2->ps_datatype) {
	reason = _T("type");
    } else if (ps1->ps_size != ps2->ps_size) {
	reason = _T("size");
    } else if (_tcscmp(ps_get_abs(ps1), ps_get_abs(ps2))) {
	reason = _T("path");
    } else if (ps_has_dcode(ps1) && ps_has_dcode(ps2)) {
	if (_tcscmp(ps1->ps_dcode, ps2->ps_dcode)) {
	    reason = _T("dcode");
	}
    } else if (moment_cmp(ps1->ps_moment, ps2->ps_moment, NULL)) {
	reason = _T("moment");
    }

    return reason;
}

/// Makes a deep copy of an existing PS with a potentially different name.
/// Warning: use carefully to avoid memory leaks.
/// @param[in] cps      the PS object pointer to be copied
/// @return the new PS object
ps_o
ps_copy(ps_o cps)
{
    ps_o nps;

    nps = ps_newFromPath(ps_get_abs(cps));
    ps_set_datatype(nps, ps_get_datatype(cps));
    ps_set_fsname(nps, ps_get_fsname(cps));
    ps_set_moment(nps, ps_get_moment(cps));
    ps_set_size(nps, ps_get_size(cps));
    ps_set_mode(nps, ps_get_mode(cps));
    ps_set_dcode(nps, ps_get_dcode(cps));
    ps_set_pn2(nps, ps_get_pn2(cps));
    ps_set_target(nps, ps_get_target(cps));

    return nps;
}

/// Format a PS for server consumption.
/// @param[in] ps       the object pointer
/// @param[out] buf     a buffer to hold the formatted string
/// @param[in] bufmax   the size of the passed-in buffer
/// @return the length of the formatted string
int
ps_toCSVString(ps_o ps, CS buf, int bufmax)
{
    CCS fsname, target;
    TCHAR modebuf[32];
    TCHAR mtime[MOMENT_BUFMAX];

    int len;

    fsname = ps_get_fsname(ps);

    (void)moment_format(ps->ps_moment, mtime, charlen(mtime));

    (void)util_format_to_radix(CSV_RADIX, modebuf, charlen(modebuf),
	ps_get_mode(ps));

    // The link target, present only for link ops. It must be URL
    // encoded in case it contains a literal comma.
    if ((target = ps_get_rel2(ps)) || (target = ps_get_target(ps))) {
	target = util_encode_minimal(target);
    }

    // *INDENT-OFF*
    len = _sntprintf(buf, bufmax,
		    _T("%c%s")			// 1 - DATATYPE
		    _T("%s%s")			// 2 - FSNAME
		    _T("%s%s")			// 2 - MTIME
		    _T("%") _T(PRIu64) _T("%s")	// 4 - SIZE
		    _T("%s%s")			// 5 - MODE
		    _T("%s%s")			// 6 - DCODE
		    _T("%s%s")			// 7 - LINK TARGET
		    _T("%s"),			// 8 - PNAME
	    ps->ps_datatype, FS1,
	    (fsname && *fsname) ? fsname : _T("?"), FS1,
	    mtime, FS1,
	    ps_get_size(ps), FS1,
	    modebuf, FS1,
	    ps_get_dcode(ps), FS1,
	    target ? target : _T(""), FS1,
	    ps_get_rel(ps));
    // *INDENT-ON*

    putil_free(target);

    if (len < 0 || len > bufmax) {
	buf[bufmax - 1] = '\0';
	putil_syserr(0, buf);
	len = _tcslen(buf);
    }

    return len;
}

/// Format a PS for human consumption.
/// The resulting string contains a trailing newline.
/// @param[in] ps       the object pointer
/// @param[in] lflag    boolean - true for long format
/// @param[in] sflag    boolean - true for short format
/// @param[out] buf     a buffer to hold the formatted string
/// @param[in] bufmax   the size of the passed-in buffer
/// @return the length of the formatted string
int
ps_format_user(ps_o ps, int lflag, int sflag, CS buf, int bufmax)
{
    CCS path;

    int len;

    path = prop_is_true(P_ABSOLUTE_PATHS) ? ps_get_abs(ps) : ps_get_rel(ps);

    buf[0] = '\0';

    if (sflag) {
	len = _sntprintf(buf, bufmax, _T("%-7s %s\n"), ps_get_dcode(ps), path);
    } else if (lflag) {
	TCHAR mtime[MOMENT_BUFMAX];

	(void)moment_format(ps->ps_moment, mtime, charlen(mtime));

	len = _sntprintf(buf, bufmax, _T("dcode=%-7s size=%-10") _T(PRIu64)
			 _T(" moment=%s %s\n"),
			 ps_get_dcode(ps),
			 ps_get_size(ps),
			 mtime,
			 path);
    } else {
	len = _sntprintf(buf, bufmax, _T("%-7s %-10") _T(PRIu64) _T(" %s\n"),
			 ps_get_dcode(ps),
			 ps_get_size(ps),
			 path);
    }

    if (len < 0 || len > bufmax) {
	buf[bufmax - 1] = '\0';
	putil_syserr(0, buf);
	len = _tcslen(buf);
    }

    return len;
}

/// Format a PathState for user consumption (typically debugging).
/// @param[in] ps       the object pointer
/// @return an allocated string which must be freed by the caller
CCS
ps_tostring(ps_o ps)
{
    TCHAR line[(PATH_MAX * 2) + 256];

    line[0] = '\0';

    (void)ps_toCSVString(ps, line, charlen(line));

    return putil_strdup(line);
}

/// Sets the moment field by parsing a stringified form.
/// @param[in] ps       the object pointer
/// @param[in] str      a stringified form of the moment_s format
/// @return 0 on success
int
ps_set_moment_str(ps_o ps, CCS str)
{
    return moment_parse(&(ps->ps_moment), str);
}

/// Sets the size field by parsing a stringified form.
/// @param[in] ps       the object pointer
/// @param[in] str      a stringified form of the size
/// @return 0 on success
void
ps_set_size_str(ps_o ps, CCS str)
{
    ps_set_size(ps, putil_strtoll(str, NULL, 10));
}

/// Mark the object as having been unlinked.
/// @param[in] ps       the object pointer
/// @return 0 on success
void
ps_set_unlinked(ps_o ps)
{
    ps_set_datatype(ps, PS_UNLINK);
}

/// Mark the object as representing a symbolic link.
/// @param[in] ps       the object pointer
/// @return 0 on success
void
ps_set_symlinked(ps_o ps)
{
    ps_set_datatype(ps, PS_SYMLINK);
}

/// Mark the object as representing a link.
/// @param[in] ps       the object pointer
/// @return 0 on success
void
ps_set_linked(ps_o ps)
{
    ps_set_datatype(ps, PS_LINK);
}

/// Mark the object as representing a directory.
/// @param[in] ps       the object pointer
/// @return 0 on success
void
ps_set_dir(ps_o ps)
{
    ps_set_datatype(ps, PS_DIR);
}

/// Gets the absolute version of 'path2' (used only for links).
/// @param[in] ps       the object pointer
/// @return a string representing the absolute path
CCS
ps_get_abs2(ps_o ps)
{
    if (ps->ps_pn2) {
	return pn_get_abs(ps->ps_pn2);
    } else {
	return NULL;
    }
}

/// Gets the project-relative version of 'path2' (used only for links).
/// @param[in] ps       the object pointer
/// @return a string representing the relative path
CCS
ps_get_rel2(ps_o ps)
{
    if (ps->ps_pn2) {
	return pn_get_rel(ps->ps_pn2);
    } else {
	return NULL;
    }
}

/// Sets the type of data associated with the given PS. This is closely
/// related to the Unix file-type divisions (regular, dir, symlink, etc)
/// but is intended to be platform neutral.
/// @param[in] ps       the object pointer
/// @param[in] datatype the data type
void
ps_set_datatype(ps_o ps, int datatype)
{
    ps->ps_datatype = (ps_e) datatype;
}

/// Gets the type of data associated with the given PS.
/// @param[in] ps       the object pointer
/// @return the data type
int
ps_get_datatype(ps_o ps)
{
    return (int)ps->ps_datatype;
}

// *INDENT-OFF*
GEN_SETTER_GETTER_DEFN(ps, moment, moment_s)
GEN_SETTER_GETTER_DEFN(ps, size, int64_t)
GEN_SETTER_GETTER_DEFN_STR(ps, dcode, CCS, PS_NO_DCODE)
GEN_SETTER_GETTER_DEFN(ps, mode, mode_t)

GEN_SETTER_GETTER_DEFN_STR(ps, fsname, CCS, NULL)
GEN_SETTER_GETTER_DEFN(ps, pn,  pn_o)
GEN_SETTER_GETTER_DEFN(ps, pn2, pn_o)
GEN_SETTER_GETTER_DEFN_STR(ps, target, CCS, NULL)

GEN_DELEGATE_GETTER_DEFN(ps, pn, abs, CCS)
GEN_DELEGATE_GETTER_DEFN(ps, pn, rel, CCS)
// *INDENT-ON*

/// Finalizer - releases all allocated memory for object.
/// If object is a null pointer, no action occurs.
/// @param[in] ps       the object pointer
void
ps_destroy(ps_o ps)
{
    if (!ps) {
	return;
    }
    // Destroy any contained PN objects.
    if (ps->ps_pn) {
	pn_destroy(ps->ps_pn);
    }
    if (ps->ps_pn2) {
	pn_destroy(ps->ps_pn2);
    }
    // Free all string pointers.
    putil_free(ps->ps_dcode);
    putil_free(ps->ps_fsname);
    putil_free(ps->ps_target);

    // Zero-fill the struct before freeing it - nicer for debugging.
    memset(ps, 0, sizeof(*ps));

    // Then free it.
    putil_free(ps);
}
