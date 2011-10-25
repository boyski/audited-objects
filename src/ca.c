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
/// @brief Definitions for the CmdAction "class" and CmdKey "inner class".
/// The CmdAction class records a command and the files it read from or
/// wrote to (see pa.c).
///
/// The term CmdAction is often abbreviated as CA.
///
/// The CmdKey class is used only as a hash
/// key for looking up CmdActions in external hashes.

#include "AO.h"

#include "CA.h"
#include "CODE.h"
#include "PA.h"
#include "PN.h"
#include "PS.h"

#include "dict.h"
#include "hash.h"

#include "zlib.h"

#if defined(_WIN32)
#include <winsock2.h>
#endif	/*_WIN32*/

/// @cond static
#define AUDIT_BUF_INIT_SIZE			8192
#define CSV_NEWLINE_TOKEN			"^J"
/// @endcond static

/// Structure for passing multiple data items into a callback.
/// Required because callback functions allow only one data pointer.
typedef struct {
    CS fp_buf;			///< Pointer to buffer
    int fp_bufsize;		///< Current size of buffer
    int fp_bufnext;		///< Index of the current buffer end
} format_palist_s;

/// Structure to serve as a key into a hash table containing cmd_audits.
// This carries a subset of CA attrs; specifically it holds
// those attributes which are common between a CA and a PA, because
// we use it to take a PA and look up its associated CA in the table.
struct cmd_key_s {
    CCS ck_ccode;		///< same as ca_o attribute
    unsigned long ck_depth;	///< same as ca_o attribute
    unsigned long ck_cmdid;	///< same as ca_o attribute
} cmd_key_s;

/// Data structure for describing audited commands.
/// PIDs are represented as unsigned long because their native
/// types are all over the map. The best way to avoid warnings
/// in formatting is to use unsigned long and "%lu" across the board.
struct cmd_audit_s {
    unsigned long ca_cmdid;	///< id of cmd (related to pid)
    unsigned long ca_depth;	///< # of execs from start to here
    unsigned long ca_pcmdid;	///< id of parent cmd
    moment_s ca_starttime;	///< start time
    unsigned long ca_duration;	///< invocation run time
    CCS ca_prog;		///< name of running program
    CCS ca_host;		///< system CA was generated on
    CCS ca_recycled;		///< PTX recycled from or NULL
    CCS ca_rwd;			///< the project relative wd
    CCS ca_pccode;		///< identity hash of parent cmd
    CCS ca_ccode;		///< identity hash of this cmd
    CCS ca_pathcode;		///< hash of all opened pathnames
    CCS ca_line;		///< the command line
    dict_t *ca_raw_pa_dict;	///< ptr to path action set
    dict_t *ca_cooked_pa_dict;	///< ptr to path action set
    hash_t *ca_group_hash;	///< ptr to aggregation hash
    ca_o ca_leader;		///< ptr to group leader
    int ca_strong;		///< boolean - is aggregation strong?
    int ca_started;		///< boolean - has this CA seen SOA?
    int ca_closed;		///< boolean - has this CA seen EOA?
    int ca_processed;		///< boolean - has this CA been fully handled?
    CCS ca_subs;		///< aggregated subcmds
} cmd_audit_s;

/// Constructor for CmdKey class. A CmdKey is a small object which
/// functions as the key to a full CmdAction in a hash table.
/// @param[in] ccode    the command code of the CmdAction
/// @param[in] depth    the depth of the CmdAction
/// @param[in] cmdid    the cmdid of the CmdAction
/// @return a CmdKey object pointer
ck_o
ck_new(CCS ccode, unsigned long depth, unsigned long cmdid)
{
    ck_o ck;

    ck = (ck_o)putil_calloc(1, sizeof(*ck));

    ck->ck_ccode = putil_strdup(ccode);
    ck->ck_depth = depth;
    ck->ck_cmdid = cmdid;

    return ck;
}

/// Constructor for CmdKey class. Given an existing CmdAction object,
/// create a CmdKey suitable for placing it in an external hash table.
/// @param[in] ca       a CmdAction object from which to build the CmdKey
/// @return a CmdKey object pointer
ck_o
ck_new_from_ca(ca_o ca)
{
    return ck_new(ca_get_ccode(ca), ca->ca_depth, ca->ca_cmdid);
}

/// A method for changing the command ID of a CmdKey.
/// @param[in] ck       the CmdKey object pointer
/// @param[in] cmdid    the new command ID
void
ck_set_cmdid(ck_o ck, unsigned long cmdid)
{
    ck->ck_cmdid = cmdid;
}

// Internal service routine. Comparator for ccodes.
// Provides a relatively random but stable ordering.
static int
_ca_cmp_codes(CCS ccode1, CCS ccode2)
{
    return strcmp(ccode1, ccode2);
}

/// Finalizer for CmdKey class - releases all allocated memory.
/// @param[in] ck       the CmdKey object pointer
void
ck_destroy(ck_o ck)
{
    putil_free(ck->ck_ccode);
    memset(ck, 0, sizeof(*ck));
    putil_free(ck);
}

/// Ordering function (NOT method) for CAs in a hash table.
/// @param[in] left     pointer to a CmdKey object
/// @param[in] right    pointer to another CmdKey object
/// @return 1, 0, or -1 as left is greater than, equal to, or less than right
int
ca_o_hash_cmp(const void *left, const void *right)
{
    ck_o ck1, ck2;
    int ccmp;

    // Typically two different CAs will differ in their 'ccode' but it's
    // quite common for the same cmd to occur in different parts of the
    // cmd tree. An obvious example would be the cmd "make" in a recursive
    // make model. Also, consider "date >> log" at the beginning and
    // end of a build; each has the build script as its parent.
    // In these cases the pids will be different so we compare those. There
    // remains the case (on Unix) where a cmd execs itself without a fork.
    // Though this would often be an infinite loop it's not necessarily
    // suicidal because the loop could be broken via an EV or a flag file.
    // In this case all the above would match and we must use the depth,
    // which will increment with each exec. I believe the combination
    // of these 3 items is safely unique.
    // The only possible issues I can think of with this design are:
    // 1. It's conceivable that two *different* processes could have the
    // same cmdid because pids are only guaranteed unique while the process
    // is running and audits are processed after they die. However,
    // given the amount of time required for pid wrap on most systems,
    // and that not only would that have to occur but the ccode and
    // depth would have to match too, this seems practically impossible.
    // 2. There's also a tiny (ISTR 1 in 10 million?) chance of
    // different cmds having the same ccode. But when that's multiplied
    // by the odds of the cmdid and depth also colliding, again this
    // seems discountable in practice.
    // Update: actually it turns out we can't ever safely rely on depth,
    // because a user is allowed to do a partial build within a subtree
    // of the project. So depth can be useful for intra-build resolution
    // but never for *inter*-build comparisons. Therefore the depth
    // test mentioned above is turned off.
    // Note: a possible improvement in robustness would be for the CA to
    // store its ccode, its parent's ccode, AND its grandparent's ccode.
    // Having a record of all three would enable this comparator to
    // compare parent ccodes as well. The reason for the grandparent
    // ccode is that in working our way up the tree we would have
    // a (ccode, pccode) pair available for each node.

    ck1 = (ck_o)left;
    ck2 = (ck_o)right;

    if ((ccmp = _ca_cmp_codes(ck1->ck_ccode, ck2->ck_ccode))) {
	// A random but deterministic sorting order.
	return ccmp;
    } else if (ck1->ck_depth != ck2->ck_depth) {
	return ck1->ck_depth - ck2->ck_depth;
    } else if (ck1->ck_cmdid == 0 || ck2->ck_cmdid == 0) {
	// In some cases we may need to drop the cmdid out of the comparison.
	return 0;
    } else if (ck1->ck_cmdid != ck2->ck_cmdid) {
	return ck1->ck_cmdid - ck2->ck_cmdid;
    } else {
	return 0;
    }
}

/// Hashing function (NOT method) for CAs (returns a hash key).
/// @param[in] key      the key to be hashed
/// @return a hash value
hash_val_t
ca_o_hash_func(const void *key)
{
    ck_o ck;
    hash_val_t ckhash;

    ck = (ck_o)key;
    ckhash = util_hash_fun_default(ck->ck_ccode);
    return ckhash;
}

/// Constructor for CmdAction class.
/// @return a new, empty CmdAction object
ca_o
ca_new()
{
    ca_o ca;

    ca = (ca_o)putil_calloc(1, sizeof(*ca));

    if (!(ca->ca_raw_pa_dict = dict_create(DICTCOUNT_T_MAX, pa_cmp))) {
	putil_syserr(2, "dict_create()");
    }

    return ca;
}

/// Constructor for CmdAction class.
/// Any change to the CSV format requires an equivalent change here.
/// @param[in] csv      a string in the canonical CSV format for CAs 
/// @return a new, complete CmdAction object
ca_o
ca_newFromCSVString(CCS csv)
{
    CS linebuf, original;
    CCS cmdid, depth, pcmdid, starttime, duration, prog, host;
    CCS recycled, rwd, pccode, ccode, pathcode;
    ca_o ca;

    // It would be nice if we could use sscanf here but it won't work
    // because sscanf can't handle multiple %s fields.

    // The line buffer will be trashed by util_strsep(), so we
    // allocate a temporary copy. The pointer passed in will also
    // be changed, so we need to hold a copy of the original address
    // to be freed.
    linebuf = original = putil_strdup(csv);

    // *INDENT-OFF*
    if (    !(cmdid      = util_strsep(&linebuf, FS1)) ||
	    !(depth      = util_strsep(&linebuf, FS1)) ||
	    !(pcmdid     = util_strsep(&linebuf, FS1)) ||
	    !(starttime  = util_strsep(&linebuf, FS1)) ||
	    !(duration   = util_strsep(&linebuf, FS1)) ||
	    !(host       = util_strsep(&linebuf, FS1)) ||
	    !(recycled   = util_strsep(&linebuf, FS1)) ||
	    !(prog       = util_strsep(&linebuf, FS1)) ||
	    !(rwd        = util_strsep(&linebuf, FS1)) ||
	    !(pccode     = util_strsep(&linebuf, FS1)) ||
	    !(ccode      = util_strsep(&linebuf, FS1)) ||
	    !(pathcode   = util_strsep(&linebuf, FS1))) {
	putil_int("bad format: '%s'", csv);
	putil_free(original);
	return NULL;
    }
    // *INDENT-ON*

    ca = ca_new();

    ca_set_cmdid(ca, strtoul(cmdid, NULL, 10));
    ca_set_depth(ca, strtoul(depth, NULL, 10));
    ca_set_pcmdid(ca, strtoul(pcmdid, NULL, 10));
    if (moment_parse(&(ca->ca_starttime), starttime)) {
	putil_int("bad moment format: %s", starttime);
	putil_free(original);
	return NULL;
    }
    ca_set_duration(ca, strtoul(duration, NULL, 10));
    ca_set_prog(ca, prog);
    ca_set_host(ca, host);
    ca_set_recycled(ca, recycled);
    ca_set_rwd(ca, rwd);
    ca_set_pccode(ca, pccode);
    ca_set_pathcode(ca, pathcode);
    ca_set_line(ca, linebuf);

    if (_ca_cmp_codes(ca_get_ccode(ca), ccode)) {
	putil_int("%s: ccode skew (%s != %s)",
		  ca_get_line(ca), ca_get_ccode(ca), ccode);
    }

    putil_free(original);
    return ca;
}

// Internal service routine to make verbosity easier.
static void
_ca_verbosity_ag(ca_o ca, CCS action, CCS text)
{
    // Checking the verbosity bit twice here is actually an optimization.
    if (vb_bitmatch(VB_AG)) {
#if 0
	vb_printf(VB_AG, "%s: '%.50s' (%s=>%s==>%s.%lu"), action,
		  text ? text : ca_get_line(ca),
		  ca_get_pccode(ca), ca_get_ccode(ca),
		  ca_get_pathcode(ca), ca_get_depth(ca));
#else
	vb_printf(VB_AG, "%s: '%.60s'", action,
		  text ? text : ca_get_line(ca));
#endif
    }
}

// Internal service routine to make verbosity easier.
static void
_ca_verbosity_pa(pa_o pa, ca_o ca, CCS action)
{
    // Checking the verbosity bit twice here is actually an optimization.
    if (vb_bitmatch(VB_PA)) {
	moment_s timestamp;

	timestamp = pa_get_timestamp(pa);
	if (timestamp.ntv_sec) {
	    char tstamp[MOMENT_BUFMAX];

	    (void)moment_format(timestamp, tstamp, charlen(tstamp));
	    vb_printf(VB_PA, "%s %c %s: (%s %s)", action,
		      pa_get_op(pa), ca_get_prog(ca),
		      tstamp, pa_get_abs(pa));
	}
    }
}

/// Tests whether the object has a valid "path code".
/// @param[in] ca       the object pointer
/// @return true if the CA has a pathcode.
int
ca_has_pathcode(ca_o ca)
{
    return !CSV_FIELD_IS_NULL(ca_get_pathcode(ca));
}

/// Tests whether this CA has no parent and thus represents
/// the top-level cmd.
/// @param[in] ca       the object pointer
/// @return true if the CA represents the top level command
int
ca_is_top(ca_o ca)
{
    return CSV_FIELD_IS_NULL(ca_get_pccode(ca));
}

/// Traverses the CA, calling the specified function on each 'raw' PA.
/// @param[in] ca       the object pointer
/// @param[in] process  pointer to the callback function
/// @param[in] data     pointer which is passed to the callback
/// @return -1 on error, sum of process() return codes on success
int
ca_foreach_raw_pa(ca_o ca, int (*process) (pa_o, void *), void *data)
{
    dict_t *dict;
    dnode_t *dnp;
    pa_o pa;
    int rc = 0, pret;

    dict = ca->ca_raw_pa_dict;
    for (dnp = dict_first(dict); dnp; dnp = dict_next(dict, dnp)) {
	pa = (pa_o)dnode_getkey(dnp);
	pret = process(pa, data);
	if (pret < 0) {
	    putil_int("error from ca_foreach_raw_pa()");
	    return -1;
	} else {
	    rc += pret;
	}
    }

    return rc;
}

/// Traverses the CA, calling the specified function on each 'cooked' PA.
/// In order to enforce the conventional ordering of read-before-write
/// we go around twice, handling reads on the first pass and everything
/// else on the second.
/// @param[in] ca       the object pointer
/// @param[in] process  pointer to the callback function
/// @param[in] data     pointer which is passed to the callback
/// @return -1 on error, sum of process() return codes on success
int
ca_foreach_cooked_pa(ca_o ca, int (*process) (pa_o, void *), void *data)
{
    dict_t *dict;
    dnode_t *dnp;
    pa_o pa;
    int rc = 0, pret;

    if ((dict = ca->ca_cooked_pa_dict)) {
	for (dnp = dict_first(dict); dnp; dnp = dict_next(dict, dnp)) {
	    pa = (pa_o)dnode_getkey(dnp);
	    if (pa_is_read(pa)) {
		pret = process(pa, data);
		if (pret < 0) {
		    putil_int("ca_foreach_cooked_pa(%s)", pa_get_abs(pa));
		    return -1;
		} else {
		    rc += pret;
		}
	    }
	}
	for (dnp = dict_first(dict); dnp; dnp = dict_next(dict, dnp)) {
	    pa = (pa_o)dnode_getkey(dnp);
	    if (!pa_is_read(pa)) {
		pret = process(pa, data);
		if (pret < 0) {
		    putil_int("ca_foreach_cooked_pa(%s)", pa_get_abs(pa));
		    return -1;
		} else {
		    rc += pret;
		}
	    }
	}
    }

    return rc;
}

/// Serializes the CmdAction and writes it to the specified file descriptor.
/// On Windows this must send to a socket. On Unix it can use any
/// kind of file descriptor.
/// @param[in] ca       the object pointer
/// @param[in] fd       file descriptor to which the serialized form is sent
void
ca_write(ca_o ca, int fd)
{
    dict_t *dict;
    dnode_t *dnp, *next;
    pa_o pa;
    char line[(PATH_MAX * 2) + 256];
    int len;

    dict = ca->ca_raw_pa_dict;

    if (dict_count(dict) <= 0) {
	return;
    }

    // In case buffered files were left open by sloppy audited programs.
    fflush(NULL);

    for (dnp = dict_first(dict); dnp;) {
	pa = (pa_o)dnode_getkey(dnp);

	// Unfortunately it's too early to dcode any written files
	// since they may still be open. But we should get stats of
	// non-member read ops since they may differ in the event
	// of a distributed build. We must assume logical coherence
	// for all write ops, member and non-member.
	// In other words, read ops are allowed to be on physically
	// separate files with the same path (e.g. /usr/include/stdio.h)
	// but write ops to the same path are assumed to be to a shared file.
	// This matters in the case of a distributed build.
	if (pa_is_read(pa) && !pa_is_member(pa)) {
	    (void)pa_stat(pa, 0);
	}

	len = pa_toCSVString(pa, line, charlen(line));
	if (len > 0) {
	    if (write(fd, line, len) == -1) {
		putil_syserr(0, "write()");
	    }
	}

	// Dictionary bookkeeping.
	next = dict_next(dict, dnp);

	dict_delete(dict, dnp);
	dnp = next;

	pa_destroy(pa);
    }
}

/// Convert all raw PAs in the group into a single set of "cooked" PAs
/// under the leader.
/// @param[in] ca       the object pointer
void
ca_coalesce(ca_o ca)
{
    dict_t *dict_raw, *dict_cooked;
    dnode_t *dnpr, *dnpc, *next;

    dict_raw = ca->ca_raw_pa_dict;

    assert(!ca->ca_cooked_pa_dict);
    if ((dict_cooked = dict_create(DICTCOUNT_T_MAX, pa_cmp_by_pathname))) {
	ca->ca_cooked_pa_dict = dict_cooked;
    } else {
	putil_syserr(2, "dict_create()");
    }

    for (dnpr = dict_first(dict_raw); dnpr;) {
	pa_o raw_pa, ckd_pa;
	op_e raw_op, ckd_op;

	next = dict_next(dict_raw, dnpr);

	raw_pa = (pa_o)dnode_getkey(dnpr);
	raw_op = pa_get_op(raw_pa);

	_ca_verbosity_pa(raw_pa, ca, "COALESCING");

	// All data is in the key - that's why the value can be null.
	if ((dnpc = dict_lookup(dict_cooked, raw_pa))) {
	    int keep_cooked = 0;

	    ckd_pa = (pa_o)dnode_getkey(dnpc);
	    ckd_op = pa_get_op(ckd_pa);

	    if (!pa_is_read(raw_pa) && !pa_is_read(ckd_pa)) {
		moment_s raw_timestamp, ckd_timestamp;

		// If they're both destructive ops (non-read) then we
		// need to consider timestamps and use the later one.

		if (pa_has_timestamp(raw_pa) && pa_has_timestamp(ckd_pa)) {
		    // If the PA's have their own timestamps, use them.
		    raw_timestamp = pa_get_timestamp(raw_pa);
		    ckd_timestamp = pa_get_timestamp(ckd_pa);
		} else {
		    // Otherwise key off the file times. This is for
		    // support of "dummy" PAs as used in shopping.
		    raw_timestamp = pa_get_moment(raw_pa);
		    ckd_timestamp = pa_get_moment(ckd_pa);
		}

		if (moment_cmp(raw_timestamp, ckd_timestamp, NULL) <= 0) {
		    // Cooked write op is newer and can stay.
		    keep_cooked = 1;
		}
	    } else if (pa_is_read(raw_pa)) {
		// There's no point replacing a read with another read,
		// so regardless of whether the current cooked PA is a
		// read or write, it can stay.
		keep_cooked = 1;
	    } else {
		// A write always beats a read.
	    }

	    if (!keep_cooked) {
		dict_delete(dict_cooked, dnpc);
		dnode_destroy(dnpc);
		_ca_verbosity_pa(ckd_pa, ca, "REMOVING");
		pa_destroy(ckd_pa);
		if (!(dnpc = dnode_create(NULL))) {
		    putil_syserr(2, "dnode_create()");
		}
		ckd_pa = pa_copy(raw_pa);
		dict_insert(dict_cooked, dnpc, ckd_pa);
	    }
	} else {
	    if (!(dnpc = dnode_create(NULL))) {
		putil_syserr(2, "dnode_create()");
	    }
	    ckd_pa = pa_copy(raw_pa);
	    dict_insert(dict_cooked, dnpc, ckd_pa);
	}

	// Clean up the raw set as we move PAs to the cooked one.
	dict_delete(dict_raw, dnpr);
	dnode_destroy(dnpr);
	dnpr = next;
    }

    return;
}

/// Merge two CmdActions, a 'leader' and a 'donor', together. The donor
/// is then redundant.
/// @param[in] leader   the leader object pointer
/// @param[in] donor    the donor object pointer
void
ca_merge(ca_o leader, ca_o donor)
{
    CCS sub;
    dict_t *dict;
    dnode_t *dnp, *next;
    pa_o pa;

    // ASSUMPTION: aggregated CAs are composed from
    // commands run serially - i.e. any parallelism
    // is above the build-script level.

    // Stringify the donor and leave that string representation in
    // the leader for the record.
    sub = ca_format_header(donor);
    if (leader->ca_subs) {
	leader->ca_subs = (CCS)putil_realloc((void *)leader->ca_subs,
					     (strlen(leader->ca_subs) +
					      strlen(sub) +
					      1) * CHARSIZE);
	strcat((CS)leader->ca_subs, sub);
    } else {
	leader->ca_subs = (CCS)putil_malloc((strlen(sub) + 1) * CHARSIZE);
	strcpy((CS)leader->ca_subs, sub);
    }
    putil_free(sub);

    // Traverse the donor's raw list of PA's and move them into the leader's.
    if ((dict = donor->ca_raw_pa_dict)) {
	for (dnp = dict_first(dict); dnp;) {
	    next = dict_next(dict, dnp);

	    pa = (pa_o)dnode_getkey(dnp);
	    ca_record_pa(leader, pa);
	    dict_delete(dict, dnp);
	    dnode_destroy(dnp);
	    dnp = next;
	}
	dict_destroy(donor->ca_raw_pa_dict);
	donor->ca_raw_pa_dict = NULL;
    }
}

/// Add a new PathAction into the specified CmdAction.
/// No coalescing is attempted - PA objects are are always added
/// here unless they are truly identical (same pointer == same object).
/// @param[in] ca       the CA object pointer
/// @param[in] pa       the PA object pointer
void
ca_record_pa(ca_o ca, pa_o pa)
{
    dnode_t *dnp;

    _ca_verbosity_pa(pa, ca, "RECORDING");

    // All data is in the key - that's why the value can be null.
    if (!(dnp = dnode_create(NULL))) {
	putil_syserr(2, "dnode_create()");
    }
    dict_insert(ca->ca_raw_pa_dict, dnp, pa);
}

/// Returns the number of raw PathActions held in the CmdAction.
/// @param[in] ca       the object pointer
/// @return the number of raw PathActions
int
ca_get_pa_count(ca_o ca)
{
    return (int)dict_count(ca->ca_raw_pa_dict);
}

/// Start up a new group with the provided CA as the leader.
/// @param[in] ca       the object pointer
/// @param[in] strength indicates the power of the aggregate's binding
void
ca_start_group(ca_o ca, int strength)
{
    assert(!ca->ca_group_hash);

    ca->ca_group_hash =
	hash_create(HASHCOUNT_T_MAX, ca_o_hash_cmp, ca_o_hash_func);
    if (!ca->ca_group_hash) {
	putil_syserr(2, "hash_create()");
    }
    // Mark the leader as a member of its own club.
    ca_set_leader(ca, ca);

    ca_set_strong(ca, strength);
}

/// Adds a new CA to the audit group.
/// @param[in] ldr      the leader object reference
/// @param[in] sub      the new member CA
void
ca_aggregate(ca_o ldr, ca_o sub)
{
    ck_o ck;
    hnode_t *hnp;

    assert(ldr->ca_group_hash);

    ck = ck_new_from_ca(sub);

    if (hash_lookup(ldr->ca_group_hash, ck)) {
	ck_destroy(ck);
    } else {
	if (!(hnp = hnode_create(sub))) {
	    putil_int("hnode_create()");
	}
	hash_insert(ldr->ca_group_hash, hnp, ck);
    }

    // Each member of the audit group gets a pointer back to the group leader.
    ca_set_leader(sub, ldr);
}

/// Returns true if the CA has a known leader.
/// @param[in] ca       the object pointer
/// @return true if a leader is known
int
ca_has_leader(ca_o ca)
{
    return ca_get_leader(ca) != NULL;
}

/// Checks the list of PAs and tells us whether any of them references
/// a file which needs to be uploaded.
/// @param[in] ca       the object pointer
/// @return true if the object refers to an uploadable file 
int
ca_is_uploadable(ca_o ca)
{
    dict_t *dict;
    dnode_t *dnp;
    pa_o pa;
    int rc = 0;

    if ((dict = ca->ca_cooked_pa_dict)) {
	for (dnp = dict_first(dict); dnp; dnp = dict_next(dict, dnp)) {
	    pa = (pa_o)dnode_getkey(dnp);
	    if (pa_get_uploadable(pa)) {
		rc++;
	    }
	}
    }

    return rc;
}

/// Breaks up the aggregation and uploads each piece separately.
/// Audit groups do not nest. Therefore, when we see an aggregating 
/// cmd while already within an audit group, the "outer" group becomes
/// invalid. This method traverses an audit group, publishes each of
/// its audits individually, and destroys the group, thus clearing
/// the way for the "inner" group to take over.
/// @param[in] ldr      the leader object pointer
/// @param[in] process  a function pointer to the publishing callback
void
ca_disband(ca_o ldr, void (*process) (ca_o))
{
    hnode_t *hnp;
    ck_o ck;
    ca_o sub;
    hscan_t hscan;

    _ca_verbosity_ag(ldr, "DISBANDING", NULL);

    hash_scan_begin(&hscan, ldr->ca_group_hash);
    for (hnp = hash_scan_next(&hscan); hnp; hnp = hash_scan_next(&hscan)) {
	ck = (ck_o)hnode_getkey(hnp);
	sub = (ca_o)hnode_get(hnp);
	if (ca_get_closed(sub)) {
	    _ca_verbosity_ag(sub, "PROCESSING", NULL);
	    ca_coalesce(sub);
	    process(sub);
	} else {
	    _ca_verbosity_ag(sub, "RELEASING", NULL);
	}
	ca_set_leader(sub, NULL);
	hash_scan_delete(ldr->ca_group_hash, hnp);
	hnode_destroy(hnp);
	ck_destroy(ck);
    }

    if (ca_get_closed(ldr)) {
	_ca_verbosity_ag(ldr, "PROCESSING", NULL);
	ca_coalesce(ldr);
	process(ldr);
    } else {
	_ca_verbosity_ag(ldr, "RELEASING", NULL);
    }

    hash_destroy(ldr->ca_group_hash);
    ldr->ca_group_hash = NULL;
    ca_set_leader(ldr, NULL);
}

/// Finish off the CA by formatting it and sending it off.
/// @param[in] ca       the object pointer
/// @param[in] process  a function pointer to the publishing callback
void
ca_publish(ca_o ca, void (*process) (ca_o))
{
    _ca_verbosity_ag(ca, "BUNDLING", NULL);

    // Combine the textual command lines of the subcommands for record keeping.
    if (ca->ca_group_hash) {
	hscan_t hscan;
	hnode_t *hnp;
	ck_o ck;
	ca_o sub;

	hash_scan_begin(&hscan, ca->ca_group_hash);
	for (hnp = hash_scan_next(&hscan); hnp; hnp = hash_scan_next(&hscan)) {
	    ck = (ck_o)hnode_getkey(hnp);
	    sub = (ca_o)hnode_get(hnp);
	    if (sub != ca) {
		_ca_verbosity_ag(sub, "MERGING", NULL);
		ca_merge(ca, sub);
		ca_set_processed(sub, 1);
	    }
	    hash_scan_delete(ca->ca_group_hash, hnp);
	    hnode_destroy(hnp);
	    ck_destroy(ck);
	}

	hash_destroy(ca->ca_group_hash);
	ca->ca_group_hash = NULL;
    }

    // Merge all PAs in subcommands in with the PA set of the leader.
    // Must be done before serializing for upload but after subcmds
    // are merged in.
    ca_coalesce(ca);

    // The sub CAs have now been sucked dry. Publish the fully merged leader.
    process(ca);
}

/// Accumulates the set and number of pathnames "involved in" a CA.
typedef struct {
    CS pca_buf;			///< Stretchable buffer.
    size_t pca_buflen;		///< Current length of stretchable buffer.
    int pca_count;		///< Number of pathnames represented.
} pathcode_accumulator_s;

// We specifically exclude unlink ops from the pathcode because some "rm"
// programs, notably on Solaris, stat each arg and only issue an unlink
// if the stat succeeds. This would make the pathcode non-deterministic.
// By ignoring unlinks we ensure that rm commands will always have the
// same pathcode but will still differ in command line.
// An rm cmd will almost always be a "must run" anyway so matching
// it isn't critical. It's important to avoid larding up the server
// state with a bunch of rm commands which are the same aside from
// differing pathcode values.
// Also, we consider only member files in the pathcode calculation. The
// reason is a bit subtle: since the underlying implementations of
// tools (compilers, headers, etc) differ per platform, the same
// "logical" command would generate a different pathcode on each platform
// and thus be seen as a different command. E.g. "include <stddef.h>
// is likely to open a different set of files on Linux than Solaris
// even for the same command in the same place in the build tree.
// It's more intuitive and requires less database churn to treat
// these as the same command with different prerequisites. Considering
// only member files removes the OS from the identity calculation,
// while still preserving the differences in the form of prereqs.
static int
_ca_add_to_pathcode(pa_o pa, void *data)
{
    if (pa_is_member(pa) && !pa_is_unlink(pa)) {
	pathcode_accumulator_s *pcap;
	CCS path;
	size_t plen;

	pcap = (pathcode_accumulator_s *)data;

	path = pa_get_abs(pa);
	plen = strlen(path);

	pcap->pca_buf = (CS)putil_realloc(pcap->pca_buf,
					  pcap->pca_buflen + plen +
					  CHARSIZE);
	strcpy(pcap->pca_buf + pcap->pca_buflen, path);

	pcap->pca_buflen += plen;
	pcap->pca_count++;
    }

    return 0;
}

/// Generates the 'pathcode' - a hash of all pathnames opened by this cmd.
/// The combination of command line and pathcode is believed
/// sufficent to uniquely identify any given command, because
/// if two commands with identical textual representations operate on the
/// same set of files, they must be functionally identical.
/// In other words the command line says what to do and the pathcode
/// says who it's done to. Thus 'touch foo' might be repeated in different
/// places but it's only the *same command* if the 'foo' it touches
/// is at the same absolute path. Similarly, 'make' might occur all
/// over in a recursive-make build but it's not the same make unless it
/// opens the same Makefile. Another way of putting it: if a cmd addresses
/// any files relative to CWD, this will be seen in the absolute paths
/// of the files it opens. If it doesn't then the CWD doesn't matter.
/// Of course there can still be semantically identical cmds which look
/// different: consider "rm foo bar" and "rm bar foo". But these cases
/// would be very rare in real life and don't break anything - we would
/// just miss a potential recycle opportunity.
/// @param[in] ca       the object pointer
void
ca_derive_pathcode(ca_o ca)
{
    pathcode_accumulator_s pca;

    pca.pca_buf = NULL;
    pca.pca_buflen = 0;
    pca.pca_count = 0;

    (void)ca_foreach_cooked_pa(ca, _ca_add_to_pathcode, &pca);

    if (pca.pca_buf) {
	char pcbuf[CODE_IDENTITY_HASH_MAX_LEN];

	(void)code_from_str(pca.pca_buf, pcbuf, charlen(pcbuf));
	putil_free(pca.pca_buf);
	snprintf(endof(pcbuf), leftlen(pcbuf), "-%d", pca.pca_count);
	ca_set_pathcode(ca, pcbuf);
    } else {
	ca_set_pathcode(ca, NULL);
    }
}

/// Returns an allocated string representing the CA without its PAs.
/// @param[in] ca       the object pointer
/// @return a string which should be freed when no longer needed
CCS
ca_format_header(ca_o ca)
{
    CS hdr, p;
    char started[MOMENT_BUFMAX];
    int ret;

    (void)moment_format(ca->ca_starttime, started, charlen(started));

    // *INDENT-OFF*
    ret = asprintf(&hdr,
			  "%lu%s"			// 1  - PID
			  "%lu%s"			// 2  - DEPTH
			  "%lu%s"			// 3  - PPID
			  "%s%s"			// 4  - STARTED
			  "%lu%s"			// 5  - DURATION
			  "%s%s"			// 6  - HOST
			  "%s%s"			// 7  - RECYCLED
			  "%s%s"			// 8  - PROG
			  "%s%s"			// 9  - RWD
			  "%s%s"			// 10 - PCCODE
			  "%s%s"			// 11 - CCODE
			  "%s%s"			// 12 - PATHCODE
			  "%s@",			// 13 - CMDLINE
	ca->ca_cmdid, FS1,
	ca->ca_depth, FS1,
	ca->ca_pcmdid, FS1,
	started, FS1,
	ca->ca_duration, FS1,
	ca->ca_host ? ca->ca_host : "?", FS1,
	ca->ca_recycled ? ca->ca_recycled : "", FS1,
	ca->ca_prog, FS1,
	ca->ca_rwd ? ca->ca_rwd : ".", FS1,
	ca_get_pccode(ca), FS1,
	ca_get_ccode(ca), FS1,
	ca_get_pathcode(ca), FS1,
	ca->ca_line);
    // *INDENT-ON*

    if (ret < 0) {
	putil_syserr(2, NULL);
    }

    // Since a CSV line must be a SINGLE line, we cannot tolerate embedded
    // newlines and have to replace them with a conventional token.
    // This should be a rare occurrence so we don't worry too much
    // about efficiency here.
    if (strchr(hdr, '\n')) {
	CS nhdr, o, n;

	nhdr = (CS)putil_calloc(strlen(hdr) * 2, CHARSIZE);
	for (o = hdr, n = nhdr; *o; o++) {
	    if (*o == '\n') {
		strcpy(n, CSV_NEWLINE_TOKEN);
		n = endof(n);
	    } else {
		*n++ = *o;
	    }
	}
	putil_free(hdr);
	hdr = (CS)putil_realloc(nhdr, strlen(nhdr) + CHARSIZE);
    }

    // We know this is present because we put it there.
    p = strrchr(hdr, '@');
    *p = '\n';

    return hdr;
}

// Internal service routine.
static int
_ca_format_palist_callback(pa_o pa, void *data)
{
    format_palist_s *fps;
    long dcode_all;
    char line[(PATH_MAX * 2) + 256];
    int len;

    fps = (format_palist_s *) data;

    dcode_all = prop_is_true(P_DCODE_ALL);

    // Use size=0 as an indicator that this path hasn't been statted yet.
    // This may lead to the occasional double stat of a file which is
    // in fact zero length but that should be insignificant.
    if (!pa_is_unlink(pa) && (pa_get_size(pa) == 0 || dcode_all)) {
	int dcode_path;

	dcode_path = dcode_all || pa_is_member(pa) || pa_get_uploadable(pa);

#if !defined(_WIN32)
	// If we're checksumming a PA which represents a write op,
	// flush the file descriptor associated with it in an
	// attempt to avoid bad dcode values caused by files left
	// open by the audited process with data still not
	// synced to disk. This is not actually known to be a
	// problem but it seems like fsync is a good idea here.
	if (dcode_path && (pa_get_fd(pa) > 2) && pa_is_write(pa)) {
	    (void)fsync(pa_get_fd(pa));
	    // IDEA: if this isn't sufficient, potentially we could
	    // go so far as to close and reopen the same fd in append
	    // mode (whether the original open was for append or not,
	    // by this point any data sent to the fd will be appended).
	    // I believe fcntl() could help us find the mode bits.
	}
#endif	/*_WIN32*/

	(void)pa_stat(pa, dcode_path);
    }

    // Skip any record which does not represent a regular file or dir.
    // This is kind of a hack since it happens awfully late -
    // it would be cleaner to keep these entries out of the set
    // from the beginning rather than ignoring them at delivery time.
    // But we're trying to limit the number of redundant/unnecessary
    // stat calls and for now at least this seems most efficient.
    if (pa_is_special(pa)) {
	return 0;
    }

    // No point recording directory reads.
    if (pa_is_dir(pa) && pa_is_read(pa)) {
	return 0;
    }

    len = pa_toCSVString(pa, line, charlen(line));

    // Stretch the buffer to fit the new data and append it.
    if (len > 0) {
	while ((fps->fp_bufsize - fps->fp_bufnext) <= len) {
	    fps->fp_bufsize *= 2;
	    fps->fp_buf = (CS)putil_realloc(fps->fp_buf, fps->fp_bufsize);
	}
	strcpy(fps->fp_buf + fps->fp_bufnext, line);
	fps->fp_bufnext += len;
    }

    return 0;
}

// Internal service routine. Returns an allocated string representing
// all PAs contained in the CA.
static CCS
_ca_format_cooked_palist(ca_o ca)
{
    format_palist_s fpstruct;

    fpstruct.fp_bufsize = AUDIT_BUF_INIT_SIZE;
    fpstruct.fp_buf = (CS)putil_malloc(fpstruct.fp_bufsize);
    fpstruct.fp_buf[0] = '\0';
    fpstruct.fp_bufnext = 0;

    (void)ca_foreach_cooked_pa(ca, _ca_format_palist_callback, &fpstruct);

    return fpstruct.fp_buf;
}

/// Formats a string representing a human-readable form of the CA.
/// @param[in] ca       the object pointer
/// @return a string which should be freed when no longer needed
CCS
ca_toCSVString(ca_o ca)
{
    CCS str, subs, pas;
    size_t len;

    str = ca_format_header(ca);
    len = strlen(str);

    subs = ca_get_subs(ca);
    len += subs ? strlen(subs) : 0;

    pas = _ca_format_cooked_palist(ca);
    len += strlen(pas);

    str = (CCS)putil_realloc((void *)str, (len + 1) * CHARSIZE);

    if (subs) {
	strcat((CS)str, subs);
    }

    strcat((CS)str, pas);
    putil_free(pas);

    return str;
}

// Internal service routine. Support ca_dump() for debug purposes.
static int
_ca_dump_pa(pa_o pa, void *data)
{
    CCS pfx, str;

    pfx = (CCS)data;
    str = pa_tostring(pa);
    fputs(pfx, stderr);
    fputs(str, stderr);
    putil_free(str);

    return 0;
}

/// For debugging support - dumps contents of a CA to stderr
/// @param[in] ca       the object pointer
/*static*/ void
ca_dump(ca_o ca)
{
    CCS hdr;
    CCS stars = "******************************************************\n";
    CCS subsep = "@@@@@@@@@@@\n";

    assert(ca);

    fputs(stars, stderr);

    hdr = ca_format_header(ca);
    fputs(hdr, stderr);
    putil_free(hdr);

    if (ca->ca_raw_pa_dict) {
	(void)ca_foreach_raw_pa(ca, _ca_dump_pa, "RAW: ");
    }

    if (ca->ca_cooked_pa_dict) {
	(void)ca_foreach_cooked_pa(ca, _ca_dump_pa, "COOKED: ");
    }

    if (ca->ca_group_hash) {
	hscan_t hscan;
	hnode_t *hnp;

	hash_scan_begin(&hscan, ca->ca_group_hash);
	for (hnp = hash_scan_next(&hscan); hnp; hnp = hash_scan_next(&hscan)) {
	    ca_o sub;

	    fputs(subsep, stderr);
	    sub = (ca_o)hnode_get(hnp);
	    ca_dump(sub);
	}
    }

    fputs(stars, stderr);
}

/// Sets the command-line field for the CmdAction object. The string
/// passed in may be freed afterward since a copy is held.
/// The 'ccode' attribute is set as a side effect since it's derived
/// from the command line.
/// @param[in] ca       the object pointer
/// @param[in] line     the new command line
void
ca_set_line(ca_o ca, CCS line)
{
    putil_free(ca->ca_line);

    if (line) {
	CS l1, l2;
	size_t nlen;

	// Turn newline tokens back into actual newlines.
	ca->ca_line = (CCS)putil_malloc((strlen(line) + 1) * CHARSIZE);
	nlen = strlen(CSV_NEWLINE_TOKEN);
	for (l1 = (CS)line, l2 = (CS)ca->ca_line; *l1; ) {
	    if (l1[0] == CSV_NEWLINE_TOKEN[0] &&
		    !strncmp(l1, CSV_NEWLINE_TOKEN, nlen)) {
		l1 += nlen;
		*l2++ = '\n';
	    } else {
		*l2++ = *l1++;
	    }
	}
	*l2 = '\0';
    } else {
	ca->ca_line = NULL;
    }

    // Determine the "command code" (aka ccode).
    if (ca->ca_line) {
	char ccbuf[CODE_IDENTITY_HASH_MAX_LEN];

	(void)code_from_str(ca->ca_line, ccbuf, sizeof(ccbuf));
	if (ccbuf[0]) {
	    snprintf(endof(ccbuf), leftlen(ccbuf),
		       "+%lu", (unsigned long)strlen(ca->ca_line));
	    ca->ca_ccode = putil_strdup(ccbuf);
	} else {
	    ca->ca_ccode = NULL;
	}
    }
}

/// Gets the command-line field from the CmdAction object.
/// @param[in] ca       the object pointer
/// @return a string representing the command line
CCS
ca_get_line(ca_o ca)
{
    return ca->ca_line;
}

/// Gets the aggregated subcommands of this CA in string form.
/// @param[in] ca       the object pointer
/// @return a string representing the command lines of all sub-commands
CCS
ca_get_subs(ca_o ca)
{
    return ca->ca_subs;
}

/// Returns the number of pending (not yet closed) audits in the group.
/// @param[in] ca       the object pointer
/// @return the number of pending audits
int
ca_get_pending(ca_o ca)
{
    int pending = 0;

    if (ca->ca_group_hash) {
	hscan_t hscan;
	hnode_t *hnp;

	hash_scan_begin(&hscan, ca->ca_group_hash);
	for (hnp = hash_scan_next(&hscan); hnp; hnp = hash_scan_next(&hscan)) {
	    ca_o sub;

	    sub = (ca_o)hnode_get(hnp);
	    if (!ca_get_closed(sub)) {
		pending++;
	    }
	}
    }

    return pending;
}

// *INDENT-OFF*
GEN_SETTER_GETTER_DEFN(ca, cmdid, unsigned long)
GEN_SETTER_GETTER_DEFN(ca, depth, unsigned long)
GEN_SETTER_GETTER_DEFN(ca, pcmdid, unsigned long)
GEN_SETTER_GETTER_DEFN(ca, starttime, moment_s)
GEN_SETTER_GETTER_DEFN(ca, duration, unsigned long)
GEN_SETTER_GETTER_DEFN_STR(ca, prog, CCS, NULL)
GEN_SETTER_GETTER_DEFN_STR(ca, host, CCS, NULL)
GEN_SETTER_GETTER_DEFN_STR(ca, recycled, CCS, NULL)
GEN_SETTER_GETTER_DEFN_STR(ca, rwd, CCS, NULL)
// Would it make sense to replace the ccode/pccode tandem
// with pathcode/pccode? It would be more elegant and compact if so.
// Actually, why not have the ccode be <cmdline hash>.<pathcode>,
// thus combining ccode and pathcode.  Then we would simply
// propagate that as pccode and dump pathcode.
GEN_SETTER_GETTER_DEFN_STR(ca, pccode, CCS, CSV_NULL_FIELD)
GEN_SETTER_GETTER_DEFN_STR(ca, ccode, CCS, CSV_NULL_FIELD)
GEN_SETTER_GETTER_DEFN_STR(ca, pathcode, CCS, CSV_NULL_FIELD)
GEN_SETTER_GETTER_DEFN(ca, leader, ca_o)
GEN_SETTER_GETTER_DEFN(ca, strong, int)
GEN_SETTER_GETTER_DEFN(ca, started, int)
GEN_SETTER_GETTER_DEFN(ca, closed, int)
GEN_SETTER_GETTER_DEFN(ca, processed, int)
// *INDENT-ON*

/// Clear out the raw set and cooked sets. Destroy the entire
/// cooked data structure but leave the raw one present though
/// empty. This is the same as the initial (post-creation) state.
/// @param[in] ca       the object pointer
void
ca_clear_pa(ca_o ca)
{
    dict_t *dict;
    dnode_t *dnp, *next;
    pa_o pa;

    if ((dict = ca->ca_raw_pa_dict)) {
	for (dnp = dict_first(dict); dnp;) {
	    next = dict_next(dict, dnp);

	    pa = (pa_o)dnode_getkey(dnp);
	    dict_delete(dict, dnp);
	    dnode_destroy(dnp);
	    pa_destroy(pa);
	    dnp = next;
	}
    }

    if ((dict = ca->ca_cooked_pa_dict)) {
	for (dnp = dict_first(dict); dnp;) {
	    next = dict_next(dict, dnp);

	    pa = (pa_o)dnode_getkey(dnp);
	    dict_delete(dict, dnp);
	    dnode_destroy(dnp);
	    pa_destroy(pa);
	    dnp = next;
	}
	dict_destroy(ca->ca_cooked_pa_dict);
	ca->ca_cooked_pa_dict = NULL;
    }
}

/// Finalizer - releases all allocated memory for object.
/// If object is a null pointer, no action occurs.
/// @param[in] ca       the object pointer
void
ca_destroy(ca_o ca)
{
    hash_t *hash;

    if (!ca) {
	return;
    }

    if ((hash = ca->ca_group_hash)) {
	if (hash_count(hash)) {
	    putil_warn("group destroyed with %d audits left",
		       (int)hash_count(hash));
	}
	hash_destroy(ca->ca_group_hash);
	ca->ca_group_hash = NULL;
    }

    ca_clear_pa(ca);

    dict_destroy(ca->ca_raw_pa_dict);

    putil_free(ca->ca_prog);
    putil_free(ca->ca_host);
    putil_free(ca->ca_recycled);
    putil_free(ca->ca_rwd);
    putil_free(ca->ca_pccode);
    putil_free(ca->ca_ccode);
    putil_free(ca->ca_pathcode);
    putil_free(ca->ca_subs);
    putil_free(ca->ca_line);
    memset(ca, 0, sizeof(*ca));
    putil_free(ca);
}
