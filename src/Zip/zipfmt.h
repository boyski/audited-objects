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

#ifndef	ZIPFMT_H__
#define	ZIPFMT_H__

/* ZIP file format definitions.
   Note that this file defines static variables! */

/* Using only "unsigned char" as an 8-bit value; <stdint.h> is perhaps not
   portable enough yet. */
typedef unsigned char byte;

/* Per-file header, followed by file data */
struct file_header {
    byte signature[4];		/* See file_header_signature */
    byte min_version[2];
    byte flags[2];
    byte compression[2];
    byte mtime[2];
    byte mdate[2];
    byte crc32[4];
    byte compressed_size[4];
    byte uncompressed_size[4];
    byte name_length[2];
    byte extra_length[2];
};				/* name and "extra" follows. */

static const byte file_header_signature[4] = { '\x50', '\x4B', '\x03', '\x04' };

enum {
    FLAGS_0_HAVE_DESCRIPTOR = 1 << 3	/* file_descriptor is present */
};

/* Header of a record in "extra". */
struct extra_header {
    byte id[2];
    byte data_size[2];
};

static const byte eh_id_zip64[2] = { '\x01', '\x00' };
static const byte eh_id_ext_timestamp[2] = { '\x55', '\x54' };

/* Second per-file header, follows file data, only exists if
   FLAGS_0_HAVE_DESCRIPTOR.
   Optionally preceded by this signature: */
static const byte file_descriptor_signature[4] = { '\x50', '\x4B', '\x07', '\x08' };

struct file_descriptor_zip32 {
    byte crc32[4];
    byte compressed_size[4];
    byte uncompressed_size[4];
};

/* Used instead of file_descriptor_zip32 if eh_id_zip64 is present. */
struct file_descriptor_zip64 {
    byte crc32[4];
    byte compressed_size[8];
    byte uncompressed_size[8];
};

/* Archive extra data */
struct archive_extra_data {
    byte signature[4];		/* See archive_extra_data_signature */
    byte extra_length[4];
};				/* Extra data follows. */
static const byte archive_extra_data_signature[4] = { '\x50', '\x4B', '\x06', '\x08' };

/* Central directory file header */
struct cd_file {
    byte signature[4];		/* See cd_file_signature */
    byte creator_version[2];
    byte min_version[2];
    byte flags[2];
    byte method[2];
    byte mtime[2];
    byte mdate[2];
    byte crc32[4];
    byte compressed_size[4];
    byte uncompressed_size[4];
    byte name_length[2];
    byte extra_length[2];
    byte comment_length[2];
    byte start_disk_no[2];
    byte int_attr[2];
    byte ext_attr[4];
    byte local_hdr_offset[4];
};				/* name, extra, comment follow. */

/* Extra data FIXME: handle 1==zip64, 0x0d==unix, ? */

static const byte cd_file_signature[4] = { '\x50', '\x4B', '\x01', '\x02' };

/* Central directory signature */
struct cd_signature {
    byte signature[4];		/* See cd_signature_signature */
    byte data_length[2];
};				/* data follows. */

static const byte cd_signature_signature[4] = { '\x50', '\x4B', '\x05', '\x05' };

/* Zip64 end of central directory */
struct cd_end_zip64_v1 {
    byte signature[4];		/* See cd_end_zip64_v1_signature */
    byte cd_end_zip64_size[8];
    byte creator_version[2];
    byte min_version[2];
    byte disk_no[4];
    byte cd_start_disk_no[4];
    byte this_disk_entries[8];
    byte total_entries[8];
    byte cd_size[8];
    byte offset[8];
};				/* Extensible data follow */

static const byte cd_end_zip64_v1_signature[4] = { '\x50', '\x4B', '\x06', '\x06' };

/* Zip64 end of central directory locator */
struct cd_end_locator_zip64 {
    byte signature[4];		/* See cd_end_locator_zip64_signature */
    byte start_disk_no[4];
    byte cd_end_zip64_offset[8];
    byte total_disks[4];
};

static const byte cd_end_locator_zip64_signature[4] = { '\x50', '\x4B', '\x06', '\x07' };

/* End of central directory record */
struct cd_end {
    byte signature[4];		/* See cd_end_signature */
    byte disk_no[2];
    byte cd_start_disk_no[2];
    byte this_disk_entries[2];
    byte total_entries[2];
    byte cd_size[4];
    byte offset[4];
    byte comment_length[2];
};				/* comment follows. */

static const byte cd_end_signature[4] = { '\x50', '\x4B', '\x05', '\x06' };

/* Return 1 if a signature matches.  Make sure to avoid unaligned accesses. */
#define SIGNATURE_MATCHES(PTR, TYPE)					\
    (memcmp((const unsigned char *)(PTR) + offsetof(struct TYPE, signature), \
	    TYPE##_signature, sizeof(TYPE##_signature)) == 0)

#endif	/*ZIPFMT_H__*/
