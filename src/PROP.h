// Copyright (c) 2002-2010 David Boyce.  All rights reserved.

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

#ifndef PROP_H
#define PROP_H

/// @file
/// @brief Declarations for prop.c

/// @cond PROP
#define PROP_EXT			_T(".properties")
#define	prop_get_str			prop_get_strA
/// @endcond PROP

#include <sys/types.h>

/// The set of defined properties, not all of them published to users.
/// We keep the list in alphabetical order after a couple of special
/// cases; it makes the help output more readable, among other things.
/// <B>NOTE: THESE MUST BE KEPT IN SYNC WITH 'proptab' in prop.c!</B>
typedef enum {
    P_BADPROP = -1,
    P_APP,
    // Alphabetical from here ...
    P_ABSOLUTE_PATHS,
    P_ACTIVATION_PROG_RE,
    P_AGGREGATED_SUBCMD,
    P_AGGREGATION_LINE_BREAK_RE,
    P_AGGREGATION_LINE_STRONG_RE,
    P_AGGREGATION_LINE_WEAK_RE,
    P_AGGREGATION_PROG_BREAK_RE,
    P_AGGREGATION_PROG_STRONG_RE,
    P_AGGREGATION_PROG_WEAK_RE,
    P_AGGREGATION_STYLE,
    P_AGGRESSIVE_SERVER,
    P_AUDIT_IGNORE_PATH_RE,
    P_AUDIT_IGNORE_PROG_RE,
    P_AUDIT_ONLY,
    P_BASE_DIR,
    P_CLIENT_HOST,
    P_CLIENT_PLATFORM,
    P_CLIENT_PORT,
    P_CLIENT_TIMEOUT_SECS,
    P_DCODE_ALL,
    P_DCODE_CACHE_SECS,
    P_DEPTH,
    P_DOC_PAGER,
    P_DOWNLOAD_ONLY,
    P_EXECUTE_ONLY,
    P_IDENTITY_HASH,
    P_LEAVE_ROADMAP,
    P_LOG_FILE,
    P_LOG_FILE_TEMP,
    P_MAKE_DEPENDS,
    P_MAKE_FILE,
    P_MAKE_ONESHELL,
    P_MEMBERS_ONLY,
    P_MMAP_LARGER_THAN,
    P_NO_MONITOR,
    P_ORIGINAL_DATESTAMP,
    P_OUTPUT_FILE,
    P_PCCODE,
    P_PCMDID,
    P_PERL_CMD,
    P_PRINT_ELAPSED,
    P_PROGNAME,
    P_PROJECT_NAME,
    P_PTX_STRATEGY,
    P_REUSE_ROADMAP,
    P_ROADMAPFILE,
    P_SERVER,
    P_SERVER_CONTEXT,
    P_SERVER_LOG_LEVEL,
    P_SESSIONID,
    P_SESSION_TIMEOUT_SECS,
    P_SHOP_IGNORE_PATH_RE,
    P_SHOP_TIME_PRECISION,
    P_STRICT,
    P_STRICT_AUDIT,
    P_STRICT_DOWNLOAD,
    P_STRICT_ERROR,
    P_STRICT_UPLOAD,
    P_SYNCHRONOUS_TRANSFERS,
    P_UNCOMPRESSED_TRANSFERS,
    P_UPLOAD_ONLY,
    P_UPLOAD_READS,
    P_VERBOSITY,
    P_WFLAG,
} prop_e;

extern void prop_init(CCS);
extern CCS prop_get_app(void);
extern void prop_set_true(prop_e);
extern void prop_override_true(prop_e);
extern int prop_is_true(prop_e);
extern int prop_has_value(prop_e);
extern int prop_is_public(prop_e);
extern void prop_put_str(prop_e, CCS);
extern void prop_put_long(prop_e, long);
extern void prop_put_ulong(prop_e, unsigned long);
extern void prop_override_str(prop_e, CCS);
extern void prop_override_ulong(prop_e, unsigned long);
extern void prop_mod_str(prop_e, CCS, char *const *);
extern void prop_mod_ulong(prop_e, unsigned long, char *const *);
extern const char *prop_get_strA(prop_e);
extern unsigned long prop_get_ulong(prop_e);
extern long prop_get_long(prop_e);
extern void prop_unset(prop_e, int);
extern void prop_load(CCS, CCS);
extern prop_e prop_from_name(CCS);
extern CCS prop_value_from_name(CCS);
extern CCS prop_to_name(prop_e);
extern void prop_unexport(prop_e, int);
extern CCS prop_desc(prop_e);
extern void prop_assert(prop_e);
extern void prop_help(int, int, CCS);
extern void prop_dump(int);
extern size_t prop_new_env_block_sizeA(char *const *);
extern char *const *prop_custom_envA(char **, char *const *);
#if defined(_WIN32)
extern size_t prop_new_env_block_sizeW(WCHAR *const *);
extern WCHAR *const *prop_custom_envW(WCHAR **, WCHAR *const *);
#endif	/*_WIN32*/
extern void prop_unexport_all(void);
extern void prop_fini(void);

#endif				/*PROP_H */
