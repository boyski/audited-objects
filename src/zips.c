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
/// @brief Clear out timestamps in zip files for reliable hashes.
/// This code is specific to zipfiles and imported from 'objcmp'.

#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include "Zip/zipfmt.h"

#include "ZIPS.h"

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
_zips_clear_file_extra(unsigned char **p1, off_t *p2, size_t extra_len,
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
_zips_clear_one_file(unsigned char **p1, off_t *p2)
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
    if (_zips_clear_file_extra(&data, &size_left,
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
_zips_clear_one_cd_file(unsigned char **p1, off_t *p2)
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
    if (_zips_clear_file_extra(&data, &size_left,
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
int
zips_clear_file(void *data_, off_t total_size)
{
    unsigned char *data;
    off_t size_left;
    size_t len;

    data = (unsigned char *)data_;
    size_left = total_size;
    /* File data */
    while (size_left >= (off_t)sizeof(struct file_header)
	   && SIGNATURE_MATCHES(data, file_header)) {
	if (_zips_clear_one_file(&data, &size_left) != 0)
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
	if (_zips_clear_one_cd_file(&data, &size_left) != 0)
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
    if ((size_t)size_left >= sizeof(struct cd_end) && SIGNATURE_MATCHES(data, cd_end)) {
	    len = _get_u16(data + offsetof(struct cd_end, comment_length));
	    ADVANCE(sizeof(struct cd_end) + len);
	}
    if (size_left != 0) {
	return -1;
    }
    return 0;
}

/* Return nonzero if the data looks like a ZIP archive. */
int
zips_is_zipfile(const void *data, off_t size)
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

// vim: sw=4 ts=8 tw=100 cc=+1
