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
/// @brief Handles the git-delivery system for audit artifacts

#include "AO.h"

#include "CA.h"
#include "GIT.h"
#include "UTIL.h"

#include "zlib.h"

static char GitCmd[PATH_MAX * 4 + 256];
static FILE *GitFP;

/// Initializes git-related data structures.
void
git_init(CCS exe)
{
    CCS str;
    const char *perl;

    UNUSED(exe);

    if (!(perl = prop_get_str(P_PERL_CMD)) && !(perl = putil_getenv("PERL"))) {
	perl = "perl";
    }
    snprintf(GitCmd, charlen(GitCmd), "%s -S ao2git", perl);

    if ((str = prop_get_str(P_WFLAG))) {
	CS s2, p, t;

	s2 = (CS)str;
	while ((t = util_strsep(&s2, "\n"))) {
	    if (*t == 'g') {
		snprintf((p = endof(GitCmd)), leftlen(GitCmd), " %s", t + 2);
		if ((p = strchr(p, ','))) {
		    *p++ = ' ';
		}
	    }
	}
    }

    snprintf(endof(GitCmd), leftlen(GitCmd),
	" -branch=%s_", util_get_logname());
    moment_format_id(NULL, endof(GitCmd), leftlen(GitCmd));

    snprintf(endof(GitCmd), leftlen(GitCmd), " -");

    if (vb_bitmatch(VB_STD)) {
	fprintf(stderr, "+ %s\n", GitCmd);
    }

    if (!(GitFP = popen(GitCmd, "w"))) {
	exit(2);
    }

    return;
}

void
git_deliver(ca_o ca)
{
    CCS cmdbuf;
    
    if (GitFP && (cmdbuf = ca_toCSVString(ca))) {
	fflush(GitFP);
	if (fprintf(GitFP, "%s\n", cmdbuf) < 0) {
	    putil_syserr(0, "fprintf");
	}
	putil_free(cmdbuf);
	fflush(GitFP);
    } else {
	return;
    }

    return;
}

CCS
git_path_to_blob(CCS sha1)
{
    CS blob;
    CCS git_dir;

    if (!(git_dir = prop_get_str(P_GIT_DIR))) {
	putil_die("no Git repository");
    }

    if (putil_is_absolute(git_dir)) {
	if (asprintf(&blob, "%s/objects/%c%c/%s",
		git_dir, sha1[0], sha1[1], sha1 + 2) <= 0) {
	    putil_syserr(2, NULL);
	}
    } else {
	if (asprintf(&blob, "%s/%s/objects/%c%c/%s",
		prop_get_str(P_BASE_DIR), git_dir,
		sha1[0], sha1[1], sha1 + 2) <= 0) {
	    putil_syserr(2, NULL);
	}
    }

    return (CCS)blob;
}

void
git_store_blob(ps_o ps)
{
    CCS sha1, blob, path;
    CS blob_dir;
    int fd, ret;
    z_stream stream;
    char hdr[64];
    unsigned char cprsbuf[4096], *fdata;
    uint64_t fsize;
    struct stat64 stbuf;

    path = ps_get_abs(ps);
    sha1 = ps_get_dcode(ps);

    if ((fd = open64(path, O_RDONLY | O_BINARY)) == -1) {
	putil_syserr(2, path);
    }

    if (fstat64(fd, &stbuf)) {
	close(fd);
	putil_syserr(2, path);
    }

    fsize = stbuf.st_size;
    fdata = util_map_file(path, fd, 0, fsize);
    close(fd);

    blob = git_path_to_blob(sha1);
    if (!access(blob, F_OK)) {
	return;
    }
    blob_dir = putil_malloc(strlen(blob) + 1);
    putil_dirname(blob, blob_dir);

    if (access(blob_dir, F_OK) && putil_mkdir_p(blob_dir)) {
	putil_syserr(2, blob_dir);
    }

    if ((fd = open64(blob, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0444)) == -1) {
	putil_syserr(2, blob);
    }

    memset(&stream, 0, sizeof(stream));
    if (deflateInit(&stream, Z_BEST_SPEED) != Z_OK) {
	putil_die("unable to compress %s: %s",
	    path, stream.msg ? stream.msg : "unknown error");
    }
    stream.avail_out = sizeof(cprsbuf);
    stream.next_out = cprsbuf;

    snprintf(hdr, charlen(hdr), "blob %lu", ps_get_size(ps));
    stream.next_in = (unsigned char *)hdr;
    stream.avail_in = strlen(hdr) + 1;
    while(deflate(&stream, 0) == Z_OK);

    stream.next_in = fdata;
    stream.avail_in = (uInt)fsize;
    do {
	ret = deflate(&stream, Z_FINISH);
	if (util_write_all(fd, cprsbuf, stream.next_out - cprsbuf) < 0) {
	    putil_syserr(2, blob);
	}
	stream.avail_out = sizeof(cprsbuf);
	stream.next_out = cprsbuf;
    } while (ret == Z_OK);

    close(fd);
    util_unmap_file(fdata, fsize);
    putil_free(blob);
    putil_free(blob_dir);
}

void
git_get_blob(CCS sha1, CCS path)
{
    CCS blob;
    int fd, ret, i;
    uint64_t fsize;
    struct stat64 stbuf;
    z_stream stream;
    unsigned char cprsbuf[4096], *fdata, *p;

    blob = git_path_to_blob(sha1);
    if ((fd = open64(blob, O_RDONLY|O_BINARY)) == -1) {
	putil_syserr(2, blob);
    }
    if (fstat64(fd, &stbuf)) {
	close(fd);
	putil_syserr(2, blob);
    }
    fsize = stbuf.st_size;
    fdata = util_map_file(blob, fd, 0, fsize);
    close(fd);

    if ((fd = open64(path, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0775)) == -1) {
	putil_syserr(2, path);
    }
    memset(&stream, 0, sizeof(stream));
    stream.next_in = fdata;
    stream.avail_in = (uInt)fsize;
    if (inflateInit(&stream) != Z_OK) {
	putil_die("inflateInit()");
    }
    for (i = 0, ret = Z_OK; ret != Z_STREAM_END; i++) {
	stream.avail_out = sizeof(cprsbuf);
	stream.next_out = cprsbuf;
	ret = inflate(&stream, Z_NO_FLUSH);
	if (i == 0) {
	    p = (unsigned char *)strchr((char *)cprsbuf, '\0') + 1;
	} else {
	    p = cprsbuf;
	}
	if (util_write_all(fd, p, stream.next_out - p) < 0) {
	    putil_syserr(2, blob);
	}
    }
    if (inflateEnd(&stream) != Z_OK) {
	putil_die("inflateEnd(): %s", stream.msg ? stream.msg : "failed");
    }
    close(fd);
    util_unmap_file(fdata, fsize);
}

unsigned char *
git_map_blob(CCS blob)
{
    int fd;
    unsigned long len;
    unsigned char *fdata;
    char hdr[] = "blob ", buf[256], *p;

    if ((fd = open64(blob, O_RDONLY|O_BINARY)) == -1) {
	putil_syserr(2, blob);
    }

    if (util_read_all(fd, buf, charlen(buf)) <= 0) {
	putil_syserr(2, blob);
    }

    // Scan for the header: "blob <len>\0" ...
    memset(buf, 0, sizeof(buf));
    for (p = buf; strncmp(buf, "blob ", 5); ) {
	if (util_read_all(fd, p, 1) <= 0) {
	    putil_syserr(2, blob);
	}
	if (*p == hdr[p - buf]) {
	    p++;
	} else {
	    p = buf;
	}
    }
    for (p = buf; *p && ((size_t)(p - buf) < charlen(buf)); p++) {
	if (util_read_all(fd, p, 1) <= 0) {
	    putil_syserr(2, blob);
	}
    }

    // ... then map that much of the blob.
    len = strtoul(buf, NULL, 10);
    fdata = util_map_file(blob, fd, 0, len);
    close(fd);

    return fdata;
}

/// Unmaps the specified region of a mapped blob.
/// @param[in] fdata    A pointer to the mapped region
/// @param[in] extent   The size of the mapped region
void
git_unmap_blob(unsigned char *fdata, unsigned long extent)
{
    util_unmap_file(fdata, extent);
}

/// Finalizes git-related data structures.
void
git_fini(void)
{
    if (GitFP && pclose(GitFP)) {
	putil_syserr(2, GitCmd);
    }

    return;
}
