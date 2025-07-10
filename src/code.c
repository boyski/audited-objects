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
/// @brief Support for signature hashes, similar in concept to checksums.
/// Also known as "fingerprint" or "identity" hashes. This signature
/// hash is used primarily for file contents, possibly other things like
/// command lines.  We define a signature hash as a fast,
/// not-necessarily-cryptographically-secure hash with "good enough"
/// range and distribution such that the odds of a collision between
/// different data sets is "sufficiently low".
///
/// The odds of collision are given by the so-called birthday paradox.
/// The birthday paradox says, in round numbers, that the likelihood of
/// a collision (two different data sets with the same hash) approaches
/// 50% as the number of data sets reaches 2**(N/2) where N is the
/// number of bits in the hash (assuming it has reasonably even
/// distribution).  Thus a 32-bit hash has a 50% chance of collision
/// when there are 2**16 or 65536 items in the collection, and a 64-bit
/// hash has the same odds with 4 billion items. Of course these
/// numbers describe 50-50 odds; it would take far fewer data points
/// for the odds of a collision to be a concern.
///
/// Fortunately it's not as bad as all that. First, not every data
/// set is compared with every other; we only compare contents
/// between files of the same name. So only if the collision happens
/// to be between data sets representing different versions of the
/// same file does it represent a problem. Or to put it another way,
/// it would take 2**(N/2) versions <i>of the same file</i> before
/// the odds of a false positive reach 50 percent.
///
/// Second, in addition to the hash we also compare file sizes,
/// on the theory that files of different sizes can never be identical.
/// So now, in order to have a false match, we would need a hash
/// collision between two versions of the same file which also
/// happen to be the same size. This seems exceedingly unlikely,
/// though I do not have the math skills to come up with a number.
///
/// The original default hash algorithm was CRC32, but it seems
/// to be conventional wisdom that CRC32 is not a great identity
/// hash because its distribution is not ideal (where ideal
/// means it uses all of the 2**32 bit patterns with the same
/// likelihood and doesn't tend to favor, say, the even numbers or
/// the positive numbers or the low numbers).
///
/// I looked at a number of other 32-bit hashes and they seemed
/// equally fast.  In fact after a little more testing it emerged
/// that most were fast enough to be lost in the noise of file
/// I/O.  In other words when hashing file contents, file I/O
/// seems to use all the significant time.  OTOH I tried MD5 but
/// that was too slow. I plugged in SHA-256 which, being a 256-bit
/// hash, has extraordinary collision resistance and it seems to
/// slow down by "only" about 40%. I looked at a bunch of other
/// individual algorithms (VMAC, Tiger, etc) before I finally
/// settled on SHA-1.
///
// Special case: the Unix "ar" (and Windows "lib") format
// unfortunately includes a timestamp. Thus, archives made at
// different times will hash with different values even if
// they're semantically identical. Since the whole point of
// the identity hash is to determine whether files are semantically
// identical, we need a workaround. All files needing a data
// hash are mapped into memory and then examined; if they start
// with a magic number indicating a file type which contains
// timestamps they're sent to a special "handler" for that type.
// Otherwise we just hash the full raw contents. There are a few
// assumptions buried here, e.g. that archives will not contain
// other archives, etc. But this is pretty standard,stable stuff
// and should almost always work.

#include "Putil/putil.h"

#include <ar.h>
#include <sys/mman.h>

#if !defined(BSD)
#include <alloca.h>
#endif // BSD

#include "AO.h"

#include "CODE.h"
#include "PROP.h"
#include "ZIPS.h"

// Define inline xxhash routines with suggested tuning.
#define XXH_STATIC_LINKING_ONLY /* access advanced declarations */
#define XXH_IMPLEMENTATION      /* access definitions */
#define XXH_INLINE_ALL
#include "xxhash.h"

#include "sha1.h"
#include "zlib.h"

/// Return true iff the file looks like the kind with an embedded
/// timestamp based on its name.
/// Files with timestamps require special handling.
#define HAS_TIMESTAMP(path) (!strcmp(strchr(path, '\0') - 2, ".a") && !access(path, F_OK))

/// Initialize hash-code data structures.
void
code_init(void)
{
    // Nothing to do here.
}

/// Take a data area and length and a string buffer. Generates a hash
/// value from the data area and formats it as a string in the string
/// buffer.
/// @param[in] data     the data area to hash
/// @param[in] size     the size of the data area
/// @param[out] buf     a string buffer to format the hash into
/// @param[in] buflen   the length of the string buffer
/// @return a pointer to the buffer if successful, else NULL
static CCS
_code_hash2str(const unsigned char *data, size_t size, CS buf, size_t buflen)
{
    XXH128_hash_t hash;

    hash = XXH3_128bits(data, size);
    (void)snprintf(buf, buflen, "%016llx%016llx",
	(unsigned long long)hash.high64,
	(unsigned long long)hash.low64);

    return buf;
}

/* Return nonzero if the data looks like an archive. */
static int
_code_is_archive_file(const void *data)
{
    return data && !memcmp(data, ARMAG, SARMAG);
}

/// Return a signature hash for the supplied string.
/// @param[in] str      the string to hash
/// @param[out] buf     a buffer to format the code into
/// @param[in] buflen   the length of the passed-in buffer
/// @return a pointer to the buffer if successful, else NULL
CCS
code_from_str(CCS str, CS buf, size_t buflen)
{
    return _code_hash2str((const unsigned char *)str, strlen(str), buf, buflen);
}

// Return the dcode for an archive (static library) file.
// Contains specialized logic to skip over the timestamps which
// these files contain.
static int
_code_clear_archive_file(const unsigned char *data, off_t len)
{
    struct ar_hdr *hdr;
    unsigned char *dot;
    off_t size;

    for (dot = (unsigned char *)data + SARMAG; dot < data + len;) {
	// Read this file's header.
	hdr = (struct ar_hdr *)dot;

	// Figure out the boundary of this file within the archive,
	// rounding up to the next even number. If a padding char
	// is present it will be a newline and thus 'stable' from
	// a hashing point of view.
	if ((size = atol(hdr->ar_size))) {
	    size += size % 2;
	} else {
	    return -1;
	}

	// Zero out everything in the header except the name field, and
	// skip past it.
	// We leave the name field (renaming an archive member changes
	// its semantics and thus requires a different checksum).
	memset(dot + sizeof(hdr->ar_name), 0,
	    sizeof(*hdr) - sizeof(hdr->ar_name));

	// Move the pointer past the member header.
	dot += sizeof(*hdr);

	// Move the pointer past the member data.
	dot += size;
    }

    return 0;
}

/// True if the data looks like it might need timestamps cleared.
/// timestamps within the buffer.
/// @param[in] data     the buffer to hash
/// @param[in] size     the size of the buffer
static int
_code_needs_patching(const void *data, off_t size)
{
    return _code_is_archive_file(data) || zips_is_zipfile(data, size);
}

/// Return a signature hash for the supplied buffer. This is
/// similar to code_from_str() but does not assume the passed
/// buffer is a null-terminated string, and takes care of
/// timestamps within the buffer.
/// @param[in] data     the buffer to hash
/// @param[in] size     the size of the buffer
/// @param[in] path     the path to an existing file associated with the buffer
/// @param[out] buf     a buffer to format the code into
/// @param[in] buflen   the length of the passed-in buffer
/// @return a pointer to the buffer if successful, else NULL
CCS
code_from_buffer(const unsigned char *data, off_t size, CCS path, CS buf, size_t buflen)
{
    if (size == 0) {
	// No sense comparing this...
    } else if (_code_is_archive_file(data)) {
	if (_code_clear_archive_file(data, size)) {
	    putil_warn("corrupt archive file: %s", path);
	}
    } else if (zips_is_zipfile(data, size)) {
	if (zips_clear_file((unsigned char *)data, size)) {
	    putil_warn("corrupt zip file: %s", path);
	}
    } else if (HAS_TIMESTAMP(path)) {
	putil_warn("possible dcode on file with timestamp: %s", path);
    }

    return _code_hash2str(data, size, buf, buflen);
}

/// Derive the data code (aka d-code) of a file in string form.
/// @param[in] path     the path to an existing file
/// @param[out] buf     a buffer to write the dcode into
/// @param[in] len      the length of the passed-in buffer
/// @return a pointer to the buffer if successful, else NULL
CCS
code_from_path(CCS path, CS buf, size_t len)
{
    unsigned char *mdata = NULL;
    unsigned char *fdata = NULL;
    unsigned char *data = NULL;
    size_t size;
    unsigned long map_cutoff;
    int no_map;

    buf[0] = '\0';

    // Find out where to draw the line between mapping and reading.
    map_cutoff = prop_get_ulong(P_MMAP_LARGER_THAN);

    // Treat (unsigned long)-1 specially to mean "don't do any mapping".
    no_map = map_cutoff == ULONG_MAX;

    // Linus Torvalds says (presumably speaking of Linux) "The gcc
    // people tested it (mmap), and their cut-off point is at 30kB
    // or so. Anything smaller than that is faster to just "read()"
    // (http://lkml.org/lkml/2001/10/13/196).
    // The cutoff is presumably fairly similar for other Unix systems
    // (I hear 16kB for BSD). So we pick a size in that area
    // and use read() below that and mmap() above.
    // But all these numbers are anecdotal and subject to change.

    // It's certainly possible that a mapping will fail here if
    // the file is larger than available swap (or something like that).
    // To date there is no workaround besides "get more memory (or swap)",
    // which I think is a reasonable answer for a build machine.
    // Alternatively we could fall back to working through it in
    // page-sized chunks but that's not yet implemented.

    {
	int fd;
	struct stat64 stbuf;

	if ((fd = open64(path, O_RDONLY, 0)) == -1) {
	    putil_syserr(0, path);
	    return NULL;
	}

	if (fstat64(fd, &stbuf)) {
	    putil_syserr(0, path);
	    close(fd);
	    return NULL;
	}

	size = stbuf.st_size;

	// Don't waste time mapping a file of length 0.
	if (size == 0) {
	    data = fdata = NULL;
	} else if (size <= map_cutoff || no_map) {
	    data = mdata = putil_malloc(size);
	    if (util_read_all(fd, mdata, size) != (ssize_t)size) {
		putil_syserr(0, path);
		putil_free(mdata);
		close(fd);
		return NULL;
	    }
	} else {
	    int prot = PROT_READ;
	    size_t lookahead = 2048;

	    // The mapping may need to be writeable so we can null out timestamps.
	    // Examine the first few bytes before mapping to see if write is needed.
	    fdata = putil_malloc(lookahead);
	    if ((util_read_all(fd, fdata, lookahead) != (ssize_t)lookahead) ||
			_code_needs_patching(fdata, lookahead)) {
		prot |= PROT_WRITE;
	    }
	    putil_free(fdata);

	    // Mapping could fail if the file is too big for available swap.
	    data = fdata = (unsigned char *)mmap64(0, size, prot, MAP_PRIVATE, fd, 0);
	    if (fdata == MAP_FAILED || fdata == NULL) {
		putil_syserr(0, path);
		close(fd);
		return NULL;
	    }

#if defined(MADV_SEQUENTIAL)
	    // A potential optimization.
	    if (madvise((char *)fdata, size, MADV_SEQUENTIAL)) {
		putil_syserr(0, path);
	    }
#endif	// MADV_SEQUENTIAL

	    if (fdata) {
		vb_printf(VB_MAP, "Mapped %p (%s)", fdata, path);
	    }
	}

	// Once the mapping exists we can and should close the descriptor.
	close(fd);
    }

    // Hash the entire region in one pass.
    code_from_buffer(data, size, path, buf, len);

    // Release the buffer whether things went well or not.
    putil_free(mdata);
    if (fdata) {
	if (munmap((void *)fdata, size) == -1) {
	    putil_syserr(0, path);
	    return NULL;
	}
	vb_printf(VB_MAP, "Unmapped %p", fdata);
    }

    // An empty buffer signals a failure to derive the dcode.
    return buf[0] ? buf : NULL;
}

/// Finalize hash-code data structures.
void
code_fini(void)
{
    // Nothing to do here.
}

// vim: sw=4 ts=8 tw=100 cc=+1
