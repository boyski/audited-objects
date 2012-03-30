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
/// @brief Miscellaneous system utility functions for AO.
/// Some of these exist only to provide a more Unix-like API on Windows.

#include "AO.h"

#include "PROP.h"

#include <stdarg.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include <lmcons.h>
#include <process.h>
#include <winsock2.h>
#include <sys/utime.h>
#else				/*!_WIN32 */
#include <grp.h>
#include <pwd.h>
#include <strings.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "Interposer/preload.h"
#endif				/*!_WIN32 */

#include "pcre.h"
#include "zlib.h"

#if defined(_WIN32)
static WSADATA wsaData;
#endif	/*_WIN32*/

static char OutputFile[PATH_MAX];
static FILE *OutputStream;

/// Compares two pathnames for equality. Basically this is an alias
/// for strcmp() which compares case-insensitively on platforms where
/// filenames are typically not case sensitive. The other reason for
/// this function's existence is that its signature matches the typedef
/// "dict_comp_t" defined by Kazlib, allowing it to be used as a
/// comparator without casting.
/// @param[in] left     the "left" pathname
/// @param[in] right    the "right" pathname
/// @return 1, 0, or -1 as left is greater than, equal to, or less than right
int
util_pathcmp(const void *left, const void *right)
{
    int rc = 0;

    if (left != right) {
#if defined(_WIN32)
	rc = stricmp((CCS)left, (CCS)right);
#elif defined(__APPLE__)
	rc = strcasecmp((CCS)left, (CCS)right);
#else
	rc = strcmp((CCS)left, (CCS)right);
#endif
    }

    return rc;
}

/// An ugly Windows hack which may be simplified away someday.
/// Winsock init/fini stuff is really painful, at least as documented.
void
util_socket_lib_init(void) {
#if defined(_WIN32)
    if (!wsaData.wVersion) {
	int wsarc;

	vb_printf(VB_OFF, "WSAStartup in %ld", getpid());
	if ((wsarc = WSAStartup(MAKEWORD(1, 1), &wsaData))) {
	    putil_win32err(2, wsarc, "WSAStartup()");
	}
    }
#endif	/*_WIN32*/
}

/// An ugly Windows hack which may be simplified away someday.
/// Winsock init/fini stuff is really painful for us.
void
util_socket_lib_fini(void) {
#if defined(_WIN32)
    // MSDN says each WSAStartup should be balanced with a WSACleanup.
    // It's not clear whether it's really needed just before exit.
    if (wsaData.wVersion) {
	vb_printf(VB_OFF, "WSACleanup in %ld", getpid());
	if (WSACleanup()) {
	    putil_win32err(0, WSAGetLastError(), "WSACleanup");
	}
    }
#endif	/*_WIN32*/
}

/// Maps the specified file into memory.
/// @param[in] path     The pathname of the file to map
/// @param[in] fd       A file descriptor for the file to be mapped
/// @param[in] offset   The offset of the first byte to map
/// @param[in] extent   The amount of the file to map
/// @return a pointer to the address at which the file is mapped
unsigned char *
util_map_file(CCS path, int fd, off_t offset, uint64_t extent)
{
    unsigned char *fdata;

#if defined(_WIN32)
    HANDLE hMap;
    HANDLE fh;

    if ((fh = (HANDLE)_get_osfhandle(fd)) == INVALID_HANDLE_VALUE) {
	putil_win32err(2, GetLastError(), path);
    }

    if (!(hMap = CreateFileMapping(fh, NULL, PAGE_READONLY, 0, 0, NULL))) {
	putil_win32err(2, GetLastError(), path);
    }

    if (!(fdata = (unsigned char *)MapViewOfFile(hMap,
						 FILE_MAP_COPY, 0, offset,
						 (SIZE_T)extent))) {
	putil_win32err(2, GetLastError(), path);
    }

    // We can close both handles as soon as the mapping exists.
    if (!CloseHandle(hMap)) {
	putil_win32err(2, GetLastError(), path);
    }
    if (!CloseHandle(fh)) {
	putil_win32err(2, GetLastError(), path);
    }
#else	/*_WIN32*/
    // It's not documented whether 'private' or 'shared' mapping
    // is faster when the mapping is PROT_READ anyway, but Solaris
    // uses 'private' to map its shared libraries and we can assume
    // that that code has been carefully tuned. Git is another data
    // point which also uses 'private' mode.
    fdata = (unsigned char *)mmap64(0, extent, PROT_READ, MAP_PRIVATE, fd, offset);
    if (fdata == MAP_FAILED) {
	putil_syserr(2, path);
    }

#if defined(MADV_SEQUENTIAL)
    // Everything we do is sequential so might as well handle this
    // in one place.
    madvise((char *)fdata, extent, MADV_SEQUENTIAL);
#endif	/*MADV_SEQUENTIAL*/
#endif	/*_WIN32*/

    vb_printf(VB_MAP, "Mapped %p (%s)", fdata, path);

    return fdata;
}

/// Unmaps the specified region of a mapped file.
/// @param[in] fdata    A pointer to the mapped region
/// @param[in] extent   The size of the mapped region
void
util_unmap_file(unsigned char *fdata, uint64_t extent)
{
    if (fdata) {
#if defined(_WIN32)
	UNUSED(extent);
	if (UnmapViewOfFile(fdata) == 0) {
	    putil_win32err(2, GetLastError(), "unmap");
	}
#else	/*_WIN32*/
	if (munmap((void *)fdata, extent) == -1) {
	    putil_syserr(0, "munmap");
	}
#endif	/*_WIN32*/
	vb_printf(VB_MAP, "Unmapped %p", fdata);
    }
}

#if defined(_WIN32)
// Documented below.
CS
util_requote_argv(CS const *argv)
{
    CS word, cmdline, ptr;
    int quote, i;
    size_t cmdlen;

    // Figure out the worst-case scenario for how much space the cmdline
    // will need and allocate that much. This worst case is when each
    // char needs an escape, plus quotes, plus the trailing space or null.
    for (cmdlen =  0, i = 0; argv[i]; i++) {
	cmdlen += (strlen(argv[i]) * 2) + 3;
    }
    cmdline = (CS)putil_malloc(cmdlen);

    for (ptr = cmdline; (word = *argv); argv++) {

	// If the word needs quoting at all, use single quotes unless
	// it _contains_ a single quote, in which case use double
	// quotes and escape special chars.
	if (*word) {
	    char *p;

	    for (quote = 0, p = word; *p; p++) {
		if (!isalnum((int)*p) && !strchr("!%+,-./=:@_", *p)) {
		    quote = '"';
		    break;
		}
	    }
	} else {
	    quote = '"';
	}

	// Add opening quote if required.
	if (quote) {
	    *ptr++ = quote;
	}

	// Copy the word into the output buffer with escapes as needed.
	while (*word) {
	    if (quote && *word == quote) {
		*ptr++ = '\\';
	    }
	    *ptr++ = *word++;
	}

	// Add closing quote if required.
	if (quote) {
	    // Special case - \ quotes trailing ".
	    if (*(ptr - 1) == '\\') {
		*ptr++ = '\\';
	    }
	    *ptr++ = quote;
	}

	// Append space or terminating null.
	if (argv[1]) {
	    *ptr++ = ' ';
	} else {
	    *ptr = '\0';
	}
    }

    // Trim to fit
    cmdline = (CS)putil_realloc(cmdline, strlen(cmdline) + 1);

    return cmdline;
}
#else	/*!_WIN32*/
/// Takes an argv (which may have been derived by a shell splitting up
/// a command line) and formats it back into a command line, quoted
/// correctly such that if split by a shell the result would be the
/// original argv.
/// @param[in] argv     An array of string
/// @return a malloc-ed string suitable for passing to the shell
CS
util_requote_argv(CS const *argv)
{
    char *word, *cmdline, *ptr;
    int quote, i;
    size_t cmdlen;

    // Figure out the worst-case scenario for how much space the cmdline
    // will need and allocate that much. This worst case is when each
    // char needs an escape, plus quotes, plus the trailing space or null.
    for (cmdlen =  0, i = 0; argv[i]; i++) {
	cmdlen += (strlen(argv[i]) * 2) + 3;
    }
    cmdline = putil_malloc(cmdlen);

    // There are 4 viable POSIX quoting strategies: (1) use only
    // backslashes (no quotes), (2) use double quotes and escape
    // as needed, (3) use single quotes and special-case
    // embedded single quotes, (4) use a hybrid strategy of examining
    // the word and choosing the simplest of single, double, or none.
    // The following uses the hybrid approach since that tends to look
    // a lot more like what a human would do.

    for (ptr = cmdline; (word = *argv); argv++) {

	// If the word needs quoting at all, use single quotes unless
	// it _contains_ a single quote, in which case use double
	// quotes and escape special chars.
	if (*word) {
	    char *p;

	    for (quote = 0, p = word; *p; p++) {
		if (!isalnum((int)*p) && !strchr("!%+,-./=:@_", *p)) {
		    if (*p == '\'') {
			quote = '"';
			break;
		    } else {
			quote = '\'';
		    }
		}
	    }
	} else {
	    quote = '"';
	}

	// Add opening quote if required.
	if (quote) {
	    *ptr++ = quote;
	}

	// Copy the word into the output buffer with appropriate escapes
	// if using double-quotes.
	for (; *word; word++) {
	    if (quote == '"') {
		if (*word == '\\') {
		    if (*(word + 1) == '\n') {
			continue;
		    } else {
			*ptr++ = '\\';
		    }
		} else if (strchr("$\"\n`", *word)) {
		    *ptr++ = '\\';
		}
	    }

	    *ptr++ = *word;
	}

	// Add closing quote if required.
	if (quote) {
	    *ptr++ = quote;
	}

	// Append space or terminating null.
	if (argv[1]) {
	    *ptr++ = ' ';
	} else {
	    *ptr = '\0';
	}
    }

    // Trim to fit
    cmdline = putil_realloc(cmdline, strlen(cmdline) + 1);

    return cmdline;
}
#endif	/*!_WIN32*/

/// A wrapper over getcwd(3) which returns a malloc-ed buffer.
/// @return an allocated buffer
CCS
util_get_cwd(void)
{
    ssize_t len;
    CS cwd;

    len = putil_path_max() + 1;
    cwd = (CS)putil_malloc(len);

    if (!getcwd(cwd, len)) {
	putil_free(cwd);
	cwd = NULL;
    }

    return cwd;

}

/// Calculates the "relative working directory". This is defined
/// to be 'the cwd as measured from the project base dir'.
/// @return an allocated buffer
CCS
util_get_rwd(void)
{
    CCS cwd;
    CCS rwd = NULL;

    if ((cwd = util_get_cwd())) {
	CCS pbase;
	size_t plen;

	if ((pbase = prop_get_str(P_BASE_DIR)))
	    plen = strlen(pbase);

	if (pbase && !pathncmp(cwd, pbase, plen)) {
	    CCS p;

	    for (p = cwd + plen; *p == '/' || *p == '\\'; p++);
	    rwd = putil_strdup(p);
	} else {
	    rwd = putil_strdup(".");
	}
	putil_free(cwd);
    } else {
	putil_syserr(0, "util_get_cwd()");
    }

    return rwd;
}

/// Determines current umask and formats it in string form.
/// @param[out] curr_umask      buffer for umask in string form
/// @param[in] len              size of buffer
/// @return the passed-in buffer
CCS
util_get_umask(CS curr_umask, size_t len)
{
#if defined(_WIN32)
    snprintf(curr_umask, len, "0%lo", 02);
#else	/*_WIN32*/
    mode_t tmask = umask(0);

    umask(tmask);
    snprintf(curr_umask, len, "0%lo", (unsigned long)tmask & 07777);
#endif	/*_WIN32*/
    return (CCS)curr_umask;
}

/// Returns the login name of the current user.
/// @return a string which should NOT be freed
CCS
util_get_logname(void)
{
#if defined(_WIN32)
    static CS username;

    if (username) {
	return username;
    } else {
	char buf[UNLEN + 1];
	DWORD len = charlen(buf);

	if (GetUserName(buf, &len)) {
	    username = putil_strdup(buf);
	    return username;
	}
    }
#else				/*!_WIN32 */
    struct passwd *me;

    // We explicitly prefer the effective uid over the actual.
    // I.e. if user A is su-ed to B we treat him as B where possible.
    if ((me = getpwuid(geteuid()))) {
	if (me->pw_name) {
	    return me->pw_name;
	}
    }
#endif				/*!_WIN32 */
    return "NOBODY";
}

/// Returns the group name of the current user.
/// @return a string which should NOT be freed
CCS
util_get_groupname(void)
{
#if defined(_WIN32)
    static char groupname[SMALLBUF];

    if (groupname[0]) {
	return groupname;
    } else {
	HANDLE TokenHandle;
	DWORD tpg_len = 0;
	PTOKEN_PRIMARY_GROUP tpg;
	char pgroup[SMALLBUF], domain[SMALLBUF];
	DWORD pgroup_len = charlen(pgroup);
	DWORD domain_len = charlen(domain);
	SID_NAME_USE snu;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &TokenHandle)) {
	    GetTokenInformation(TokenHandle, TokenPrimaryGroup, NULL,
				tpg_len, &tpg_len);
	    if (tpg_len) {
		tpg = (PTOKEN_PRIMARY_GROUP) GlobalAlloc(GPTR, tpg_len);
		if (GetTokenInformation(TokenHandle, TokenPrimaryGroup,
					tpg, tpg_len, &tpg_len)) {
		    if (LookupAccountSid(NULL, tpg->PrimaryGroup,
					 pgroup, &pgroup_len, domain,
					 &domain_len, &snu)) {
			snprintf(groupname, charlen(groupname),
				   "%s/%s", domain, pgroup);
			return groupname;
		    }
		}
	    }
	}
    }
#else				/*!_WIN32 */
    struct group *me;

    if ((me = getgrgid(getegid()))) {
	if (me->gr_name) {
	    return me->gr_name;
	}
    }
#endif				/*!_WIN32 */
    return "NOGROUP";
}

/// Modifies the passed-in string in place to be all lower case.
/// @param[in,out] str  modifiable, null-terminated string
/// @return a pointer to the modified string.
CS
util_strdown(CS str)
{
    CS t;

    for (t = str; *t; t++) {
	// Windows requires this test though Unix does not.
	if (ISUPPER(*t)) {
	    *t = tolower((int)*t);
	}
    }
    return str;
}

/// Modifies the passed-in string in place to be all UPPER CASE.
/// @param[in,out] str  modifiable, null-terminated string
/// @return a pointer to the modified string.
CS
util_strup(CS str)
{
    CS t;

    for (t = str; *t; t++) {
	// Windows requires this test though Unix does not.
	if (ISLOWER(*t)) {
	    *t = toupper((int)*t);
	}
    }
    return str;
}

/// Removes whitespace from both left and right ends of the passed string.
/// @param[in,out] str  modifiable, null-terminated string
/// @return a pointer to the modified string.
CS
util_strtrim(CS str)
{
    CS t;

    for (; ISSPACE(*str); str++);
    for (t = endof(str); ISSPACE(*(t - 1)); t--);
    *t = '\0';
    return str;
}

#if defined(BSD)
#include <sys/mount.h>
#if !defined(__APPLE__)
#include <elf.h>
#endif /*APPLE*/
#else /*BSD*/
#if defined(_WIN32)
#include <windows.h>
#else	/*_WIN32*/
#include <sys/statvfs.h>
#include <sys/vfs.h>
#endif	/*_WIN32*/
#endif /*BSD*/

#if defined(linux) || defined(__CYGWIN__)
#include <search.h>
#include <sys/vfs.h>
    typedef struct {
    long f_type;
    char f_name[32];
} fstypes;

static fstypes fsmap[128];
size_t fstype_cnt;

static int
_util_linux_fstype_cmp(const void *node1, const void *node2)
{
    return (((fstypes *) node1)->f_type != ((fstypes *) node2)->f_type);
}

#endif				/*linux||__CYGWIN__*/

/// Determines the textual name of the filesystem containing the
/// specified path.
/// @param[in] path     the path to be tested
/// @param[out] buf     a buffer for the filesystem name
/// @param[in] len      size of buffer
/// @return the passed-in buffer
// TODO: possibly we should be using getmntent() on Solaris and Linux
// or getmntinfo() on BSD, but I wrote this before I found them.
CCS
util_find_fsname(CCS path, CS buf, size_t len)
{
#if defined(_WIN32)
    char vol[32768];

    strcpy(buf, "??fs");
    if (GetVolumePathName(path, vol, charlen(vol))) {
	// MSDN says we need a trailing \ here ...
	strcat(vol, "\\");
	if (GetVolumeInformation(vol, NULL, 0, NULL, NULL, NULL, buf, len)) {
	    return buf;
	}
    }
    return NULL;
#else	/*_WIN32*/
    // If path doesn't exist, return the fs name of its parent (recursively).
    if (access(path, F_OK)) {
	CS pdir;
	CCS result;

	if ((pdir = putil_dirname(path))) {
	    result = util_find_fsname(pdir, buf, len);
	    putil_free(pdir);
	    return result;
	}
    }

    (void)strcpy(buf, "??fs");
#if defined(sun)
    {
	struct statvfs fsbuf;

	if (statvfs(path, &fsbuf) == 0) {
	    (void)strcpy(buf, fsbuf.f_basetype);
	}
    }
#elif defined(BSD)
    {
	struct statfs fsbuf;

	if (statfs(path, &fsbuf) == 0) {
	    (void)strcpy(buf, fsbuf.f_fstypename);
	}
    }
#elif defined(linux) || defined(__CYGWIN__)
    {
	struct statfs sfs;
	FILE *mtab;
	fstypes tempnode, *fnd;

	if (statfs(path, &sfs) != 0) {
	    return buf;
	}
	memset(&tempnode, 0, sizeof(tempnode));
	tempnode.f_type = sfs.f_type;
	if ((fnd = lfind(&tempnode, &fsmap, &fstype_cnt,
			 sizeof(fstypes), _util_linux_fstype_cmp))) {
	    strcpy(buf, fnd->f_name);
	    return buf;
	}

	if ((mtab = fopen("/proc/mounts", "r"))) {
	    fstypes fsnode;
	    char *lnbuf, *mtbuf, *fsbuf;

	    lnbuf = putil_malloc(putil_path_max() + 1);
	    mtbuf = putil_malloc(putil_path_max() + 1);
	    fsbuf = putil_malloc(putil_path_max() + 1);

	    while (fgets(lnbuf, putil_path_max() + 1, mtab) && !feof(mtab)) {
		if (sscanf(lnbuf, "%*s %s %s", mtbuf, fsbuf) == 2) {
		    if (!strcmp(fsbuf, "rootfs")) {
			continue;
		    }
		    if (statfs(mtbuf, &sfs) == -1) {
			fprintf(stderr, "statfs(%s): %s\n",
				mtbuf, strerror(errno));
			continue;
		    }
		    memset(&fsnode, 0, sizeof(fsnode));
		    fsnode.f_type = sfs.f_type;
		    strcpy(fsnode.f_name, fsbuf);
		    lsearch(&fsnode, &fsmap, &fstype_cnt,
			    sizeof(fstypes), _util_linux_fstype_cmp);
		}
	    }
	    fclose(mtab);
	    putil_free(lnbuf);
	    putil_free(mtbuf);
	    putil_free(fsbuf);
	}

	if ((fnd = lfind(&tempnode, &fsmap, &fstype_cnt,
			 sizeof(fstypes), _util_linux_fstype_cmp))) {
	    strcpy(buf, fnd->f_name);
	}
    }
#else	/*linux||__CYGWIN__*/
    // Don't know how to get fs name on other platforms ...
    UNUSED(path);
#endif	/*linux||__CYGWIN__*/
    return buf;
#endif	/*_WIN32*/
}

/// Like the standard send() system call but takes care of
/// interrupted system calls and short writes, and guarantees
/// not to return until the entire request has been sent or
/// EOF or an error condition is encountered.
/// @param[in] fd       file descriptor to send to
/// @param[out] buf     buffer for outgoing data
/// @param[in] len      number of bytes requested to send
/// @param[in] flags    flags to pass on to send()
/// @return the number of bytes sent
ssize_t
util_send_all(SOCKET fd, const void *buf, ssize_t len, int flags)
{
    ssize_t n, total;

    for (total = 0; total < len;) {
	n = send(fd, (const char *)buf + total, len - total, flags);
	if (n < 0) {
#if defined(EINTR)
	    if (errno != EINTR) {
		return -1;
	    }
#else	     /*EINTR*/
		return -1;
#endif	     /*EINTR*/
	}
	total += n;
    }

    return total;
}

/// Reads "n" bytes from a descriptor.
/// Adapted from Unix Network Programming. Takes care of interrupted
/// system calls, short reads, etc and guarantees not to return until
/// all "n" bytes have been read, or EOF or an error is encountered.
/// @param[in] fd       file descriptor to read from
/// @param[out] vptr    buffer for incoming data
/// @param[in] n        number of bytes requested
/// @return the number of bytes read
ssize_t
util_read_all(int fd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;

    if (fd < 0)
	return -1;

    ptr = (char *)vptr;
    nleft = n;
    while (nleft > 0) {
	if ((nread = read(fd, ptr, nleft)) < 0) {
#if defined(EINTR)
	    if (errno == EINTR) {
		nread = 0;	/* and call read() again */
	    } else {
		return -1;
	    }
#else	     /*EINTR*/
		return -1;
#endif	     /*EINTR*/
	} else if (nread == 0) {
	    break;		/* EOF */
	}

	nleft -= nread;
	ptr += nread;
    }
    return n - nleft;		/* return >= 0 */
}

/// Writes "n" bytes to a descriptor.
/// Adapted from Unix Network Programming. Takes care of interrupted
/// system calls, short writes, etc and guarantees not to return until
/// all "n" bytes have been read, or EOF or an error is encountered.
/// @param[in] fd       file descriptor to read from
/// @param[in] vptr     buffer for outgoing data
/// @param[in] n        number of bytes requested
/// @return the number of bytes written
ssize_t
util_write_all(int fd, const void *vptr, size_t n)
{
    ssize_t total;
    const char *p;

    for (p = (const char *)vptr, total = 0; n > 0; ) {
	ssize_t sofar;

	for (sofar = 0; ; total += sofar) {
	    ssize_t w;

	    w = write(fd, p, n);
	    if (w < 0) {
#if defined(_WIN32)
		return -1;
#else	/*_WIN32*/
		if (errno != EAGAIN && errno != EINTR)
		    return -1;
		continue;
#endif	/*_WIN32*/
	    }
	    sofar += w;
	    break;
	}
	n -= sofar;
	p += sofar;
    }

    return total;
}

/// Certain properties are allowed to contain standard substitution strings
/// such as %u and %p which are replaced by <I>user</I> and <I>project</I>
/// respectively. If the corresponding property is not set, the token is
/// left in place without change.
/// @param[in] input      the original string
/// @param[out] replaced  pointer to potentially modified string
/// @return nonzero iff any changes were made
int
util_substitute_params(CCS input, CCS *replaced)
{
    struct utsname sysdata;
    unsigned i;
    CS buf;
    CS l;
    CCS replacement;
    int changed = 0;
    size_t len;

    len = strlen(input) + 1;
    buf = (CS)putil_calloc(len, 1);

    for (i = 0, l = buf; input[i]; i++) {
	if (input[i] == '%') {
	    switch (input[i + 1]) {
		case '%':
		    replacement = "%";
		    break;
		case 'b': case 'B':
		    replacement = prop_get_str(P_BASE_DIR);
		    break;
		case 'm': case 'M':
		    (void)putil_uname(&sysdata);
		    replacement = sysdata.machine;
		    break;
		case 'n': case 'N':
		    (void)putil_uname(&sysdata);
		    replacement = sysdata.nodename;
		    break;
		case 'p': case 'P':
		    replacement = prop_get_str(P_PROJECT_NAME);
		    break;
		case 'u': case 'U':
		    replacement = util_get_logname();
		    break;
		case 'r': case 'R':
		    (void)putil_uname(&sysdata);
		    replacement = sysdata.release;
		    break;
		case 's': case 'S':
		    (void)putil_uname(&sysdata);
		    replacement = sysdata.sysname;
		    break;
		default:
		    replacement = NULL;
		    break;
	    }

	    if (replacement) {
		size_t offset;

		offset = l - buf;
		len += strlen(replacement);
		buf = (CS)putil_realloc(buf, len);
		l = buf + offset;
		(void)memset(l, 0, len - offset);
		strcpy(l, replacement);
		if (strcmp(replacement, "%")) {
		    changed = 1;
		    if (ISALPHA(input[i + 1]) && ISUPPER(input[i + 1])) {
			util_strup(l);
		    }
		}
	    } else {
		*l++ = '%';
		*l = input[i + 1];
	    }

	    i++;
	    l = endof(l);
	} else {
	    *l++ = input[i];
	}
    }

    *replaced = buf;
    return changed;
}

/// String hashing function, borrowed from kazlib.
/// @param key  the string to hash
/// @return a hash value - see kazlib for details
unsigned long
util_hash_fun_default(const void *key)
{
    static unsigned long randbox[] = {
	0x49848f1bU, 0xe6255dbaU, 0x36da5bdcU, 0x47bf94e9U,
	0x8cbcce22U, 0x559fc06aU, 0xd268f536U, 0xe10af79aU,
	0xc1af4d69U, 0x1d2917b5U, 0xec4c304dU, 0x9ee5016cU,
	0x69232f74U, 0xfead7bb3U, 0xe9089ab6U, 0xf012f6aeU,
    };

    const unsigned char *str = (const unsigned char *)key;
    unsigned long acc = 0;

    while (*str) {
	acc ^= randbox[(*str + acc) & 0xf];
	acc = (acc << 1) | (acc >> 31);
	acc &= 0xffffffffU;
	acc ^= randbox[((*str++ >> 4) + acc) & 0xf];
	acc = (acc << 2) | (acc >> 30);
	acc &= 0xffffffffU;
    }
    return acc;
}

/// Decides whether a path should be considered a temp file.
/// On Windows this requires stepping carefully around
/// both upper/lower case and short/long name issues.
/// On Unix it's a simple matter of being in or under a
/// directory named "/tmp".
/// On both Unix and Windows we assume that a name ending
/// with ".tmp" is a temp file.
/// @param path the pathname to check
/// @return true iff the path is within a known temp dir.
int
util_is_tmp(CCS path)
{
    static const char *basedir;
    size_t baselen;
    int rc = 0;
#if defined(_WIN32)
    // This does not use TCHARs because we assume UTF-8.
    static char szTempPathLong[MAX_PATH], szTempPathShort[MAX_PATH];
#endif

    if (! basedir) {
	if ((basedir = prop_get_str(P_BASE_DIR))) {
	    baselen = strlen(basedir);
	}
    }

    // If the path is under the project's basedir, assume it
    // cannot be a temp file; this allows builds to be done in
    // a directory like, say, /var/tmp/foo/bar.
    if (basedir && !strncmp(basedir, path, baselen)) {
	return 0;
    }

#if defined(_WIN32)
    if (!szTempPathLong[0]) {
	// Record the official temp-file location so we can ignore
	// files there. This tends to be returned as a short pathname
	// so for safety's sake we convert it to both short and long
	// versions and check both (*&@# Windows!)
	if (GetTempPathA(charlen(szTempPathShort), szTempPathShort) == 0) {
	    putil_win32err(0, GetLastError(), "GetTempPath");
	}

	if (GetShortPathNameA(szTempPathShort,
			      szTempPathShort, charlen(szTempPathShort)) == 0) {
	    putil_win32err(0, GetLastError(), "GetShortPathName");
	}
	putil_canon_path(szTempPathShort, NULL, 0);

	if (GetLongPathNameA(szTempPathShort,
			     szTempPathLong, charlen(szTempPathLong)) == 0) {
	    putil_win32err(0, GetLastError(), "GetLongPathName");
	    strcpy(szTempPathLong, szTempPathShort);
	}
	putil_canon_path(szTempPathLong, NULL, 0);
    }

    if (!strnicmp(path, szTempPathLong, strlen(szTempPathLong))) {
	rc = 1;
    } else if (!strnicmp(path, szTempPathShort, strlen(szTempPathShort))) {
	rc = 1;
    } else if (!stricmp(endof(path) - 4, ".tmp")) {
	rc = 1;
    } else {
	rc = 0;
    }
#else				/*!_WIN32 */
    rc = strstr(path, "/tmp/") ||
	!util_pathcmp(endof(path) - 4, "/tmp") ||
	!util_pathcmp(endof(path) - 4, ".tmp");
#endif				/*!_WIN32 */

    return rc;
}

// Internal service routine.
static void
_util_finalize_output_file(void)
{
    if (OutputStream && !fclose(OutputStream) && OutputFile[0]) {
	CS obuf;

	if (asprintf(&obuf, "%s.%ld.tmp", OutputFile, (long)getpid()) >= 0) {
	    unlink(OutputFile);	// needed on Windows
	    if (rename(obuf, OutputFile)) {
		putil_syserr(0, OutputFile);
	    }
	    putil_free(obuf);
	}
    }
}

/// Opens the requested output file. This function takes care of
/// absolute-izing the pathname in case of a subsequent chdir and
/// of replacing standard substitution strings in the pathname.
/// It also registers a function to close the file automatically
/// at exit time. The actual file opened is a temp file which is
/// renamed to the correct name at exit.
/// @param ofile the pathname to open
/// @return a stdio FILE pointer
FILE *
util_open_output_file(CCS ofile)
{
    if (!OutputStream) {
	if (!strcmp(ofile, "-")) {
	    OutputStream = stdout;
	} else if (!strcmp(ofile, "=")) {
	    OutputStream = stderr;
	} else if (!strcmp(ofile, DEVNULL)) {
	    if (!(OutputStream = fopen(ofile, "a"))) {
		putil_syserr(2, ofile);
	    }
	} else {
	    CS abuf;
	    CS tbuf;
	    CCS obuf = NULL;

	    (void)util_substitute_params(ofile, &obuf);

	    if (!(abuf = putil_realpath(obuf, 1))) {
		putil_syserr(2, obuf);
	    }
	    putil_free(obuf);

	    prop_override_str(P_OUTPUT_FILE, abuf);

	    snprintf(OutputFile, charlen(OutputFile), "%s", abuf);

	    asprintf(&tbuf, "%s.%ld.tmp", abuf, (long)getpid());

	    if (!(OutputStream = fopen(tbuf, "a"))) {
		putil_syserr(2, tbuf);
	    }

	    putil_free(abuf);
	    putil_free(tbuf);

	    atexit(_util_finalize_output_file);
	}
    }

    return OutputStream;
}

/// Prints the time elapsed from the supplied reference time till "now".
/// @param since      the reference (start) time
/// @param minimum    if elapsed time is less than described, don't bother
/// @param msg        word to describe what "now" currently means
void
util_print_elapsed(time_t since, long minimum, CCS msg)
{
    time_t elapsed, hours, minutes, seconds;

    elapsed = time(NULL) - since;
    if (minimum && elapsed >= minimum) {
	hours = elapsed / 3600;
	seconds = elapsed % 60;
	minutes = (elapsed - (hours * 3600) - seconds + 30) / 60;
	vb_printf(VB_ON, "%s: %dh%dm%ds", msg,
		    (int)hours, (int)minutes, (int)seconds);
    }
}

/// Attempts to fire up a debugger on the current process in a new window.
/// Requires the X Windows System to be in use because it depends on
/// starting the debugger within an xterm windows.
/// This will run the debugger unless the _START_DEBUGGER
/// environment variable is present and set to 0.
void
util_debug_from_here(void)
{
    char *ev;

    if (!(ev = putil_getenv("_START_DEBUGGER")) || *ev != '0') {

#if defined(_WIN32)
	DebugBreak();
#else				/*!_WIN32 */
	char *buf;

	asprintf(&buf,
		 "set -x; %s_32= %s_64= %s= xterm -e gdb --quiet %s %lu &",
		 PRELOAD_EV, PRELOAD_EV, PRELOAD_EV,
		 putil_getexecpath(), (unsigned long)getpid());

	if (system(buf)) {
	    abort();
	} else {
	    (void)sleep(2);
	}
	putil_free(buf);
#endif				/*!_WIN32 */
    }
}

/// Format the specified number as a string in the specified radix.
/// We like to store stringified numbers in a high-numbered (36?) base
/// because it's a more compact form.
/// @param[in] radix    The desired radix (aka base)
/// @param[in] buf      A buffer into which to place the formatted value
/// @param[in] len      The size of the buffer
/// @param[in] val      The numeric value to be formatted
/// @return the format buffer
CCS
util_format_to_radix(unsigned radix, CS buf, size_t len, uint64_t val)
{
    CS p;
    uint64_t d;

/// @cond static
// It's important that lower case be used for base-36 for compatibility
// with the way Java works,
#define CHRS "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
/// @endcond static

    assert(radix < sizeof(CHRS));
    memset(buf, 0, len);
    p = (buf + len) - 1;
    do {
	assert(p >= buf);
	d = val % radix;
	*--p = CHRS[d];
	val /= radix;
    } while (val > 0);

    if (p > buf) {
	strcpy(buf, p);
    }

    return buf;
}

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/// Get next token from string *stringp, where tokens are possibly-empty
/// strings separated by characters from delim.
/// Writes NULs into the string at *stringp to end tokens.
/// delim need not remain constant from call to call.
/// On return, *stringp points past the last NUL written (if there might
/// be further tokens), or is NULL (if there are definitely no more tokens).
/// If *stringp is NULL, strsep returns NULL.
/// NOTE - this is a copy of the BSD strsep() function.
/// @param stringp      pointer to the string to chop up
/// @param delim        current delimiter string
/// @return NULL when finished
char *
util_strsep(char **stringp, const char *delim)
{
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;

    if ((s = *stringp) == NULL) {
	return (NULL);
    }

    for (tok = s; ;) {
	c = *s++;
	spanp = delim;
	do {
	    if ((sc = *spanp++) == c) {
		if (c == 0) {
		    s = NULL;
		} else {
		    s[-1] = 0;
		}
		*stringp = s;
		return (tok);
	    }
	} while (sc != 0);
    }
    /* NOTREACHED */
}

/// A partial URL-encoding function which encodes only a subset of
/// characters normally treated by a URL encoder. Currently only [,%]
/// plus newline are encoded. Commas and newlines are encoded to allow
/// a string to be used in a CSV format without breaking it, and the
/// percent sign is encoded because any URL encoder must do so.
/// @param instring    the string to be encoded
/// @return an allocated, minimally encoded string which should be freed
CS
util_encode_minimal(CCS instring)
{
    size_t inlen, outlen, i, j;
    CS outstring;

    inlen = outlen = strlen(instring) + CHARSIZE;
    outstring = (CS)putil_malloc(inlen);

    for (i = j = 0; i < inlen; i++) {
	if (instring[i] == '%' || instring[i] == ',' || instring[i] == '\n') {
	    outlen += 2 * CHARSIZE;
	    outstring = (CS)putil_realloc(outstring, outlen);
	    snprintf(&outstring[j], 4, "%%%02X", instring[i]);
	    j += 3;
	} else {
	    outstring[j++] = instring[i];
	}
    }

    return outstring;
}

/// The Zlib documentation (and API) is a mess. You have to read zlib.h
/// to get the details. This uses some hackery derived from zlib.h
/// to compress a provided buffer into a gzip-compatible format; without
/// these hacks you get a deflate format which is legal but less common
/// and less well recognized (and which gunzip does not understand).
/// @param name        a name associated with the input (pathname etc)
/// @param source      the input (uncompressed) buffer
/// @param slen        the size of the input buffer
/// @param dlen        ptr to ulong which receives final compressed length
/// @return the compressed buffer, which must be freed after use
unsigned char *
util_gzip_buffer(CCS name, unsigned const char *source, uint64_t slen, uint64_t *dlen)
{
    z_stream stream;
    unsigned char *dest;
    int rc;

    memset(&stream, 0, sizeof(stream));

    stream.next_in = (Bytef *) source;
    stream.avail_in = (uInt) slen;
    stream.avail_out = (uInt) (slen + (slen/1000 + 1) + 12);

    // We deliberately use the native allocator here because we may
    // attempt to survive a failure.
    stream.next_out = dest = (unsigned char *)malloc(stream.avail_out);
    if (!dest) {
	putil_syserr(0, "malloc()");
	return NULL;
    }

    // See zlib.h for deflateInit2/windowBits.
    rc = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
			       MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
    if (rc != Z_OK) {
	putil_warn("unable to compress %s: %s",
	    name, stream.msg ? stream.msg : "unknown error");
	putil_free(dest);
	return NULL;
    }

    while (1) {
	rc = deflate(&stream, Z_FINISH);
	if (rc == Z_STREAM_END) {
	    break;
	} else if (rc == Z_OK) {
	    if (stream.avail_out == 0) {
		// Not sure this makes sense ... it avoids an apparently
		// spurious warning on Ubuntu 9.04 x86_64, but I don't
		// understand the zlib API so it may not be the right fix.
		break;
	    }
	    stream.avail_out *= 2;
	    stream.next_out = dest =
		(unsigned char *)realloc(dest, stream.avail_out);
	    if (!dest) {
		putil_warn("unable to compress %s: %s",
		    name, stream.msg ? stream.msg : "unknown error");
		return NULL;
	    }
	} else {
	    putil_warn("unable to compress %s: %s",
		name, stream.msg ? stream.msg : "unknown error");
	}
    }

    *dlen = stream.total_out;

    if (deflateEnd(&stream) != Z_OK) {
	putil_warn("unable to compress %s: %s",
	    name, stream.msg ? stream.msg : "unknown error");
	putil_free(dest);
	return NULL;
    }

    // Shrink to fit.
    if (!(dest = (unsigned char *)putil_realloc(dest, stream.total_out))) {
	putil_syserr(0, "realloc()");
    }

    return dest;
}

/*
 * Taken from libcurl.
 */

/// Borrowed from libcurl. Safe casting.
#define ISXDIGIT(x) (isxdigit((int) ((unsigned char)x)))

/// Undoes the URL encoding of the specified string.
/// This is taken from libcurl.
/// If length == 0, the length is assumed to be strlen(string).
/// If olen == NULL, no output length is stored.
/// @param[in] string   The input string, potentially URL-encoded.
/// @param[in] length   The length of the input string, or 0 to use strlen
/// @param[out] olen    Receives the length of the output string
/// @return a malloc-ed string, unescaped
char *
util_unescape(const char *string, int length, int *olen)
{
    int alloc = (length ? length : (int)strlen(string)) + 1;
    char *ns = (char *)putil_malloc(alloc);
    unsigned char in;
    int strindex = 0;
    long hex;

    if (!ns)
	return NULL;

    while (--alloc > 0) {
	in = *string;
	if (('%' == in) && ISXDIGIT(string[1]) && ISXDIGIT(string[2])) {
	    /* this is two hexadecimal digits following a '%' */
	    char hexstr[3];
	    char *ptr;

	    hexstr[0] = string[1];
	    hexstr[1] = string[2];
	    hexstr[2] = 0;

	    hex = strtol(hexstr, &ptr, 16);

	    in = (unsigned char)hex;	/* this long is never bigger than 255 anyway */

	    string += 2;
	    alloc -= 2;
	}

	ns[strindex++] = in;
	string++;
    }
    ns[strindex] = 0;		/* terminate it */

    if (olen)
	/* store output size */
	*olen = strindex;
    return ns;
}
