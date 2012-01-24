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

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <time.h>
#include <wchar.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <winsock2.h>
#endif

#include "Putil/putil.h"

/// @file
/// @brief Declarations for util.c

/// @cond UTIL

/// The number of 100-nanosecond intervals (Windows time unit) in a second.
#define WIN_CLUNKS_PER_SEC		10000000L

/// The number of 100-nanosecond intervals ("clunks") from the Windows
/// epoch to the Unix epoch.
#define WIN_EPOCH_OFFSET		116444736000000000LL

/// @endcond UTIL

extern int util_pathcmp(const void *, const void *);
extern void util_socket_lib_init(void);
extern void util_socket_lib_fini(void);
unsigned char * util_map_file(CCS, int, off_t, uint64_t);
extern void util_unmap_file(unsigned char *, uint64_t);
extern CS util_requote_argv(CS const *);
extern CCS util_get_cwd(void);
extern CCS util_get_rwd(void);
extern CCS util_get_logname(void);
extern CCS util_get_groupname(void);
extern CCS util_get_umask(CS, size_t);
extern CS util_strup(CS);
extern CS util_strdown(CS);
extern CS util_strtrim(CS);
extern CCS util_find_fsname(CCS, CS, size_t);
extern ssize_t util_send_all(SOCKET, const void *, ssize_t, int);
extern ssize_t util_read_all(int, void *, size_t);
extern ssize_t util_write_all(int, const void *, size_t);
extern int util_substitute_params(CCS, CCS *);
extern unsigned long util_hash_fun_default(const void *);
extern int util_is_tmp(CCS);
extern FILE *util_open_output_file(CCS);
extern void util_print_elapsed(time_t, long, CCS);
extern void util_debug_from_here(void);
extern CCS util_format_to_radix(unsigned, CS, size_t, uint64_t);
extern char *util_strsep(char **, const char *);
extern CS util_encode_minimal(CCS);
extern unsigned char *util_gzip_buffer(CCS name, unsigned const char *, uint64_t, uint64_t *);
extern char *util_unescape(const char *, int, int *);

#endif				/*UTIL_H */
