// Copyright (c) 2005-2011 David Boyce.  All rights reserved.

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

#if defined(_WIN32)
#define ARMAG				IMAGE_ARCHIVE_START
#define SARMAG				IMAGE_ARCHIVE_START_SIZE
#define ARFMAG				IMAGE_ARCHIVE_END
#include "unstamp.h"
#else	/*_WIN32*/
#include <ar.h>
#include <sys/mman.h>
#endif	/*_WIN32*/

#if !defined(_WIN32) && !defined(BSD)
#include <alloca.h>
#endif	/*_WIN32*/

#include "AO.h"

#include "CODE.h"
#include "PROP.h"

#include "sha1.h"
#include "zlib.h"

/// Returns true iff the file looks like the kind with an embedded
/// timestamp based on its name.
/// Files with timestamps require special handling.
#if defined(_WIN32)
#define HAS_TIMESTAMP(path)	((!stricmp(strchr(path, '\0') - 4, ".lib") ||\
				 !stricmp(strchr(path, '\0') - 4, ".obj")  ||\
				 !stricmp(strchr(path, '\0') - 4, ".exe")) &&\
				 !access(path, F_OK))
#else				/*!_WIN32 */
#define HAS_TIMESTAMP(path)	(!strcmp(strchr(path, '\0') - 2, ".a")\
				    && !access(path, F_OK))
#endif				/*!_WIN32 */

static int _code_is_zip_file(const void *, off_t);
static int _code_clear_zip_file(void *, off_t);

/// Initializes hash-code data structures.
void
code_init(void)
{
    // Nothing to do here.
}

/// Takes a data area and length and a string buffer. Generates a hash
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
    CCS algorithm;
    uint32_t hash1;

    buf[0] = '\0';

    algorithm = prop_get_str(P_IDENTITY_HASH);

    if (algorithm && *algorithm &&
	    (!stricmp(algorithm, "sha1") || !stricmp(algorithm, "git"))) {
	SHA1Context sha;
	uint8_t Message_Digest[20];
	int err, i;

	// There's a SHA-1 implementation available via libgit2 but that
	// requires linking with openssh which drags in other stuff etc.
	// so we use a standalone sha1.c derived directly from RFC3174.
	// We may eventually need other git features from libgit2 but
	// the hope would be to use that only from the monitor; the
	// auditor should stay as small and statically linked as possible
	// and libgit2/openssh/etc make that harder.
	if ((err = SHA1Reset(&sha))) {
	    putil_die("SHA1Reset() error %d", err);
	}
	{
	    char hdr[64];

	    // A git blob hash is not a vanilla SHA-1; it has a header.
	    // For the 3 characters "XYZ" it's equivalent to these:
	    //   $ echo -ne "blob 3\0XYZ" | sha1sum
	    //   $ echo -n XYZ | git hash-object --stdin
	    snprintf(hdr, sizeof(hdr), "blob %ld", size);
	    if ((err = SHA1Input(&sha, (unsigned char *)hdr, strlen(hdr) + 1))) {
		putil_die("SHA1Input() error %d", err);
	    }
	}
	if ((err = SHA1Input(&sha, (unsigned char *)data, size))) {
	    putil_die("SHA1Input() error %d", err);
	}
	if ((err = SHA1Result(&sha, Message_Digest))) {
	    putil_die("SHA1Result() error %d", err);
	}
	for(i = 0; i < 20 ; i++) {
	    snprintf(buf + (i * 2), 3, "%02x", (CCS)(intptr_t)(Message_Digest[i]));
	}
    } else {
	// Old reliable CRC32. Said to be a bad choice for an identity
	// hash as the distribution is not great. It's not really a hash
	// algorithm at all, it's an error checker. That said, it's
	// easy to use, bundled with zlib, pretty fast, and probably
	// won't fail most of the time :-)

	hash1 = crc32(crc32(0L, Z_NULL, 0), data, size);
	(void)util_format_to_radix(CSV_RADIX, buf, buflen, hash1);
	if (algorithm && *algorithm && strnicmp(algorithm, "crc", 3)) {
	    putil_die("unrecognized digest name: %s", algorithm);
	}
    }

    // An empty string from the formatter signals a failure.
    return buf[0] ? buf : NULL;
}

/* Return nonzero if the data looks like an archive. */
static int
_code_is_archive_file(const void *data)
{
    return data && !memcmp(data, ARMAG, SARMAG);
}

/// Returns a signature hash for the supplied string.
/// @param[in] str      the string to hash
/// @param[out] buf     a buffer to format the code into
/// @param[in] buflen   the length of the passed-in buffer
/// @return a pointer to the buffer if successful, else NULL
CCS
code_from_str(CCS str, CS buf, size_t buflen)
{
    return _code_hash2str((const unsigned char *)str, strlen(str), buf, buflen);
}

// Returns the dcode for an archive (static library) file.
// Contains specialized logic to skip over the datestamps which
// these files contain.
#if defined(_WIN32)
static int
_code_clear_archive_file(const unsigned char *data, off_t len)
{
    IMAGE_ARCHIVE_MEMBER_HEADER *hdr;
    unsigned char *dot;
    off_t size;

    for (dot = (unsigned char *)data + SARMAG; dot < data + len;) {
	// Read this file's header.
	hdr = (IMAGE_ARCHIVE_MEMBER_HEADER *)dot;

	// Figure out the boundary of this file within the archive,
	// rounding up to the next even number. If a padding char
	// is present it will be a newline and thus 'stable' from
	// a hashing point of view.
	if ((size = atol((const char *)hdr->Size))) {
	    size += size % 2;
	} else {
	    return -1;
	}

	// Zero out everything in the header except the name field, and
	// skip past it.
	// We leave the name field (renaming an archive member changes
	// its semantics and thus requires a different checksum).
	memset(dot + sizeof(hdr->Name), 0, sizeof(*hdr) - sizeof(hdr->Name));

	// Move the pointer past the member header.
	dot += sizeof(*hdr);

	// Fix the individual .obj file represented by this offset.
	unstamp_mapped_file(dot);

	// On Windows an archive may be a regular static library
	// like on Unix, or it may be an "import library", which
	// is just a collection of symbols exported by an
	// accompanying DLL. The following block is to remove an
	// extra timestamp in import libraries. See pecoff.doc
	// section 8 for details.
	if (*((UINT16 *)(dot)) == IMAGE_FILE_MACHINE_UNKNOWN &&
	    *((UINT16 *)(dot + 2)) == 0xFFFF) {

	    // Zero out the version field within the import header.
	    *((UINT16 *)(dot + 4)) = 0;

	    // Zero out the timestamp field within the import header.
	    *((UINT32 *)(dot + 8)) = 0;
	}

	// Move the pointer past the member data.
	dot += size;
    }

    return 0;
}
#else				/*!_WIN32 */
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
#endif	/*_WIN32*/

#if defined(_WIN32)
/* Return nonzero if the data looks like a Windows PE or COFF file. */
static int
_code_is_PE_file(const void *data)
{
    if (!data) {
	return 0;
    } else if (((PIMAGE_DOS_HEADER)data)->e_magic == IMAGE_DOS_SIGNATURE) {
	return 1;
    } else if (((PIMAGE_FILE_HEADER)data)->Machine == IMAGE_FILE_MACHINE_I386) {
	return 1;
    } else {
	return 0;
    }
}

static int
_code_clear_PE_file(const unsigned char *data)
{
    // Contains specialized logic to skip over the datestamps which
    // PE or COFF files contain.
    unstamp_mapped_file(data);

    return 0;
}
#endif	/*_WIN32*/

static int
_code_needs_patching(const void *data, off_t size)
{
    return _code_is_archive_file(data) ||
#if defined(_WIN32)
	_code_is_PE_file(data) ||
#endif	/*_WIN32*/
	_code_is_zip_file(data, size);
}

/// Returns a signature hash for the supplied buffer. This is
/// similar to code_from_str() but does not assume the passed
/// buffer is a null-terminated string, and takes care of
/// datestamps within the buffer.
/// @param[in] data     the buffer to hash
/// @param[in] size     the size of the buffer
/// @param[in] path     the path to an existing file associated with the buffer
/// @param[out] buf     a buffer to format the code into
/// @param[in] buflen   the length of the passed-in buffer
/// @return a pointer to the buffer if successful, else NULL
CCS
code_from_buffer(const unsigned char *data, off_t size,
		 CCS path, CS buf, size_t buflen)
{
    if (size == 0) {
	// No sense comparing this...
    } else if (_code_is_archive_file(data)) {
	if (_code_clear_archive_file(data, size)) {
	    putil_warn("corrupt archive file: %s", path);
	}
    } else if (_code_is_zip_file(data, size)) {
	if (_code_clear_zip_file((unsigned char *)data, size)) {
	    putil_warn("corrupt zip file: %s", path);
	}
#if defined(_WIN32)
    } else if (_code_is_PE_file(data) || GetBinaryType(path, NULL)) {
	if (_code_clear_PE_file((unsigned char *)data)) {
	    putil_warn("corrupt PE/COFF file: %s", path);
	}
    } else if (HAS_TIMESTAMP(path) && _code_is_PE_file(data + 6)) {
	putil_warn("dcode on Windows .obj built with /GL: %s", path);
#endif	/*_WIN32*/
    } else if (HAS_TIMESTAMP(path)) {
	putil_warn("possible dcode on file with timestamp: %s", path);
    }

    return _code_hash2str(data, size, buf, buflen);
}

/// Derives the data code (aka d-code) of a file in string form.
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

    // It is certainly possible that a mapping will fail here if
    // the file is larger than available swap (or something like that).
    // To date there is no workaround besides "get more memory (or swap)",
    // which I think is a reasonable answer for a build machine.
    // Alternatively we could fall back to working through it in
    // page-sized chunks but that is not yet implemented. And this
    // would require the use of a hash algorithm which can work
    // incrementally, which not all can do.

#if defined(_WIN32)
    {
	HANDLE fh, hMap;
	DWORD protection;

	protection = PAGE_WRITECOPY;

	fh = CreateFile(path, GENERIC_READ,
			FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
	    putil_win32err(0, GetLastError(), path);
	    return NULL;
	}

	if ((size = GetFileSize(fh, NULL)) == INVALID_FILE_SIZE) {
	    putil_win32err(0, GetLastError(), path);
	    CloseHandle(fh);
	    return NULL;
	}

	// Don't waste time mapping a file of length 0.
	if (size == 0) {
	    data = fdata = NULL;
	} else if (size <= map_cutoff || no_map) {
	    DWORD actual;

	    data = mdata = (unsigned char *)putil_malloc(size);
	    if (!ReadFile(fh, mdata, size, &actual, NULL) || actual <= 0) {
		putil_win32err(0, GetLastError(), path);
		putil_free(mdata);
		CloseHandle(fh);
		return NULL;
	    }
	} else {
	    if (!(hMap = CreateFileMapping(fh, NULL, protection, 0, 0, NULL))) {
		putil_win32err(0, GetLastError(), path);
		CloseHandle(fh);
		return NULL;
	    }

	    if (!(data = fdata = (unsigned char *)MapViewOfFile(hMap,
							 FILE_MAP_COPY, 0, 0,
							 size))) {
		putil_win32err(0, GetLastError(), path);
		CloseHandle(hMap);
		CloseHandle(fh);
		return NULL;
	    }

	    // We can close this handle as soon as the mapping exists.
	    if (!CloseHandle(hMap)) {
		putil_win32err(2, GetLastError(), path);
	    }

	    if (fdata) {
		vb_printf(VB_MAP, "Mapped %p (%s)", fdata, path);
	    }
	}

	if (!CloseHandle(fh)) {
	    putil_win32err(2, GetLastError(), path);
	}
    }
#else				/*!_WIN32 */
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

	    // The mapping may need to be writeable so we can null out datestamps.
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
#endif	/*MADV_SEQUENTIAL*/

	    if (fdata) {
		vb_printf(VB_MAP, "Mapped %p (%s)", fdata, path);
	    }
	}

	// Once the mapping exists we can and should close the descriptor.
	close(fd);
    }
#endif				/*!_WIN32 */

    // Hash the entire region in one pass.
    code_from_buffer(data, size, path, buf, len);

    // Release the buffer whether things went well or not.
    if (mdata) {
	putil_free(mdata);
    }
    if (fdata) {
#if defined(_WIN32)
	if (UnmapViewOfFile(fdata) == 0) {
	    putil_win32err(0, GetLastError(), path);
	    return NULL;
	}
#else				/*!_WIN32 */
	if (munmap((void *)fdata, size) == -1) {
	    putil_syserr(0, path);
	    return NULL;
	}
#endif				/*!_WIN32 */
	vb_printf(VB_MAP, "Unmapped %p", fdata);
    }

    // An empty string here signals a failure to derive the dcode.
    return buf[0] ? buf : NULL;
}

//////////////////////////////////////////////////////////////////////
// Code below is specific to zipfiles and imported from 'objcmp'.
//////////////////////////////////////////////////////////////////////

#include "Zip/zipfmt.h"

/* Return a 16-bit value */
static unsigned
_get_u16(const byte *b)
{
    return b[0] | (b[1] << 8);
}

/* Return a 32-bit value */
static unsigned
_get_u32(const byte *b)
{
    return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

/* Return a 64-bit value. */
static unsigned long long
_get_u64(const byte *b)
{
    return _get_u32(b) | ((unsigned long long)_get_u32(b + 4) << 32);
}

/// @cond static
#define ADVANCE(SIZE)				\
    do {					\
	size_t s__ = (SIZE);			\
	if ((size_t)size_left < s__)		\
	    return -1;				\
	size_left -= s__;			\
	data += s__;				\
    } while(0)
/// @endcond static

/* Clear per-file extra data;
   Update compressed_size if it is not NULL, otherwise assume compressed_size
   is not present in the zip64 header. */
static int
_code_clear_file_extra(unsigned char **p1, off_t *p2, size_t extra_len,
	    int *is_zip64, unsigned long long *compressed_size)
{
    unsigned char *data;
    off_t size_left;

    data = *p1;
    size_left = *p2;
    while (extra_len >= sizeof(struct extra_header)) {
	struct extra_header eh;
	size_t len;

	memcpy(&eh, data, sizeof(eh));
	ADVANCE(sizeof(eh));
	len = _get_u16(eh.data_size);
	if (extra_len < sizeof(eh) + len)
	    return -1;
	extra_len -= sizeof(eh) + len;
	if (memcmp(eh.id, eh_id_zip64, sizeof(eh_id_zip64)) == 0) {
	    *is_zip64 = 1;
	    if (compressed_size != NULL)
		*compressed_size = _get_u64(data + 8);
	    ADVANCE(len);
	} else if (memcmp(eh.id, eh_id_ext_timestamp,
			  sizeof(eh_id_ext_timestamp)) == 0) {
	    if (len < 1)
		return -1;
	    ADVANCE(1);		/* Time presence flags */
	    memset(data, 0, len - 1);	/* mtime, atime, ctime - if present */
	    ADVANCE(len - 1);
	} else
	    ADVANCE(len);
    }
    if (extra_len != 0)
	return -1;
    *p1 = data;
    *p2 = size_left;
    return 0;
}

/* Clear data about a single file */
static int
_code_clear_one_file(unsigned char **p1, off_t *p2)
{
    static const byte unset[4] = { '\xFF', '\xFF', '\xFF', '\xFF' };

    unsigned char *data;
    off_t size_left;
    struct file_header h;
    size_t len;
    unsigned long long compressed_size;
    int is_zip64;

    data = *p1;
    size_left = *p2;
    memcpy(&h, data, sizeof(h));
    memset(h.mtime, 0, sizeof(h.mtime));
    memset(h.mdate, 0, sizeof(h.mdate));
    memcpy(data, &h, sizeof(h));
    ADVANCE(sizeof(h));
    compressed_size = _get_u32(h.compressed_size);
    is_zip64 = 0;
    len = _get_u16(h.name_length);
    ADVANCE(len);
    if (_code_clear_file_extra(&data, &size_left,
	    _get_u16(h.extra_length), &is_zip64,
	    (memcmp(h.uncompressed_size, unset, sizeof(unset)) == 0
	    && (memcmp(h.uncompressed_size, unset, sizeof(unset)) == 0))
	    ? &compressed_size : NULL) != 0) {
	return -1;
    }
    if (compressed_size != 0) {
	ADVANCE((size_t)compressed_size);
    } else if (h.flags[0] & FLAGS_0_HAVE_DESCRIPTOR) {
	size_t off;

	/* Just search for the descriptor; perhaps we could start with the
	   central directory and get the compressed size from there instead? */
	off = 0;
	while (off + sizeof(file_descriptor_signature)
	       + sizeof(struct file_descriptor_zip32) < (size_t)size_left) {
	    unsigned char *p;

	    p = (unsigned char *)memchr(data + off, file_descriptor_signature[0],
		       size_left - off);
	    if (p != NULL && memcmp(p, file_descriptor_signature,
				    sizeof(file_descriptor_signature)) == 0) {
		ADVANCE(p - data);
		break;
	    }
	    off = (p - data) + 1;
	}
    }
    if (h.flags[0] & FLAGS_0_HAVE_DESCRIPTOR) {
	if ((size_t)size_left >= sizeof(file_descriptor_signature)
	    && memcmp(data, file_descriptor_signature,
		      sizeof(file_descriptor_signature)) == 0)
	    ADVANCE(sizeof(file_descriptor_signature));
	if (is_zip64)
	    len = sizeof(struct file_descriptor_zip64);
	else
	    len = sizeof(struct file_descriptor_zip32);
	ADVANCE(len);
    }
    *p1 = data;
    *p2 = size_left;
    return 0;
}

/* Clear central directory data about a single file */
static int
_code_clear_one_cd_file(unsigned char **p1, off_t *p2)
{
    unsigned char *data;
    off_t size_left;
    struct cd_file h;
    size_t len;
    int is_zip64;

    data = *p1;
    size_left = *p2;
    memcpy(&h, data, sizeof(h));
    memset(h.mtime, 0, sizeof(h.mtime));
    memset(h.mdate, 0, sizeof(h.mdate));
    memcpy(data, &h, sizeof(h));
    ADVANCE(sizeof(h));
    len = _get_u16(h.name_length);
    ADVANCE(len);
    if (_code_clear_file_extra(&data, &size_left,
	    _get_u16(h.extra_length), &is_zip64, NULL) != 0) {
	return -1;
    }
    len = _get_u16(h.comment_length);
    ADVANCE(len);
    *p1 = data;
    *p2 = size_left;
    return 0;
}

/// @cond static
#undef ADVANCE
#define ADVANCE(SIZE)				\
    do {					\
	size_t s__ = (SIZE);			\
	if ((size_t)size_left < s__)		\
	    return -1;				\
	size_left -= s__;			\
	data += s__;				\
    } while(0)
/// @endcond static

/* Zero all known timestamps in data_.  Return zero if cleared successfully. */
static int
_code_clear_zip_file(void *data_, off_t total_size)
{
    unsigned char *data;
    off_t size_left;
    size_t len;

    data = (unsigned char *)data_;
    size_left = total_size;
    /* File data */
    while (size_left >= (off_t)sizeof(struct file_header)
	   && SIGNATURE_MATCHES(data, file_header)) {
	if (_code_clear_one_file(&data, &size_left) != 0)
	    return -1;
    }
    /* Archive decryption header would go here. */
    if ((size_t)size_left >= sizeof(struct archive_extra_data)
	&& SIGNATURE_MATCHES(data, archive_extra_data)) {
	len = _get_u32(data + offsetof(struct archive_extra_data, extra_length));
	ADVANCE(sizeof(struct archive_extra_data) + len);
    }
    while ((size_t)size_left >= (off_t)sizeof(struct cd_file)
	   && SIGNATURE_MATCHES(data, cd_file)) {
	if (_code_clear_one_cd_file(&data, &size_left) != 0)
	    return -1;
    }
    if ((size_t)size_left >= sizeof(struct cd_signature)
	&& SIGNATURE_MATCHES(data, cd_signature)) {
	len = _get_u16(data + offsetof(struct cd_signature, data_length));
	ADVANCE(sizeof(struct cd_signature) + len);
    }
    if ((size_t)size_left >= sizeof(struct cd_end_zip64_v1)
	&& SIGNATURE_MATCHES(data, cd_end_zip64_v1)) {
	len = 12 + (size_t)_get_u64(data + offsetof(struct cd_end_zip64_v1,
						   cd_end_zip64_size));
	ADVANCE(len);
    }
    if ((size_t)size_left >= sizeof(struct cd_end_locator_zip64)
	&& SIGNATURE_MATCHES(data, cd_end_locator_zip64))
	ADVANCE(sizeof(struct cd_end_locator_zip64));
    if ((size_t)size_left >= sizeof(struct cd_end)
	&& SIGNATURE_MATCHES(data, cd_end)) {
	len = _get_u16(data + offsetof(struct cd_end, comment_length));
	ADVANCE(sizeof(struct cd_end) + len);
    }
    if (size_left != 0) {
	return -1;
    }
    return 0;
}

/* Return nonzero if the data looks like a ZIP archive. */
static int
_code_is_zip_file(const void *data, off_t size)
{
    // Expect at least one file header and the end of central
    // directory record.
    if (size == 0) {
	return 0;
    } else {
	return size >= (off_t)(sizeof(struct file_header)
	    + sizeof(struct cd_end))
	    && SIGNATURE_MATCHES(data, file_header);
    }
}

/// Finalizes hash-code data structures.
void
code_fini(void)
{
    // Nothing to do here.
}
