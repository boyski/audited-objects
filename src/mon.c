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
/// @brief Manages the input stream of records from audited commands.
/// This file also contains the code for starting and ending PTXes
/// on the server side, managing roadmaps and logfiles, and
/// parsing audit packets as they arrive from each audited cmd.

#include "AO.h"

#include "curl/curl.h"

#include "cdb.h"
#include "hash.h"

#include "CA.h"
#include "CODE.h"
#include "DOWN.h"
#include "GIT.h"
#include "HTTP.h"
#include "MAKE.h"
#include "MON.h"
#include "PROP.h"
#include "RE.h"
#include "SHOP.h"
#include "UP.h"

#include <time.h>

/// @cond static
static hash_t *AuditHash;
static void *line_break_re, *line_strong_re, *line_weak_re;
static void *prog_break_re, *prog_strong_re, *prog_weak_re;
/// @endcond static

/// Enumeration of the different aggregation types.
typedef enum {
    AGG_NEUTRAL,		///< Stick with the current aggregation plan
    AGG_NONE,			///< No aggregation (this cmd is on its own)
    AGG_WEAK,			///< Weak aggregation (yields to children)
    AGG_STRONG,			///< Strong (rolls up all children)
    AGG_BREAK,			///< Forces an end to current aggregation
} agg_e;

/// Initializes monitor data structures.
void
mon_init(void)
{
    if (!AuditHash) {
	if (!(AuditHash =
	      hash_create(HASHCOUNT_T_MAX,
	      ca_o_hash_cmp, ca_o_hash_func))) {
	    putil_syserr(2, "hash_create()");
	}
    }

    line_break_re = re_init__(P_AGGREGATION_LINE_BREAK_RE);
    prog_break_re = re_init__(P_AGGREGATION_PROG_BREAK_RE);
    line_strong_re = re_init__(P_AGGREGATION_LINE_STRONG_RE);
    prog_strong_re = re_init__(P_AGGREGATION_PROG_STRONG_RE);
    line_weak_re = re_init__(P_AGGREGATION_LINE_WEAK_RE);
    prog_weak_re = re_init__(P_AGGREGATION_PROG_WEAK_RE);

    if (prop_is_true(P_UPLOAD_ONLY) || prop_is_true(P_AUDIT_ONLY)) {
	prop_unset(P_ROADMAPFILE, 0);
    } else {
	shop_init();
    }

    // Enable dcode cache management based on property settings.
    ps_dcode_cache_init();
}

// Internal utility function to make verbosity easier.
static void
_mon_verbosity(ca_o ca, CCS action)
{
    vb_printf(VB_CA, "%s: %s", action, ca_get_line(ca));
}

// This supports debugging/verbosity. It does not delete anything and
// returns a string which must be freed manually.
static CCS
_mon_tostring(void)
{
    ca_o ca;
    CCS str, result;
    hnode_t *hnp;
    hscan_t hscan;
    CCS sep = "----\n";

    if (AuditHash) {
	result = putil_strdup("");
	hash_scan_begin(&hscan, AuditHash);
	for (hnp = hash_scan_next(&hscan); hnp; hnp = hash_scan_next(&hscan)) {
	    size_t num;

	    ca = (ca_o)hnode_get(hnp);
	    str = ca_toCSVString(ca);
	    num = strlen(result) + strlen(str) + strlen(sep) + 1;
	    result = (CCS)putil_realloc((void *)result, num * CHARSIZE);
	    strcat((CS)result, str);
	    strcat((CS)result, sep);
	    putil_free(str);
	}
    } else {
	result = putil_strdup("(UNINITIALIZED)");
    }

    return result;
}

/// For debugging support - dumps contents of a PTX to stderr. This is not
/// really a "published" function but it must be global in order to be
/// callable from a debugger.
/*static*/ void
mon_dump(void)
{
    CCS str;
    CCS bars = "========================================================\n";

    fputs(bars, stderr);
    str = _mon_tostring();
    fputs(str, stderr);
    fputs(bars, stderr);
    putil_free(str);
}

// If this returns true, no PTX has been or will be created. We can still
// download files from the server but no state (PTX, session, etc) is left.
static int
_mon_no_ptx(void)
{
    return prop_get_ulong(P_DOWNLOAD_ONLY) == 2;
}

// Internal service routine. Callback for _mon_process_ca().
static int
_mon_process_pa(pa_o pa, void *data)
{
    UNUSED(data);

    if (!pa_get_uploadable(pa)) {
	return 0;
    } else if (prop_is_true(P_DOWNLOAD_ONLY) || prop_is_true(P_AUDIT_ONLY)) {
	return 0;
    } else {
	up_load_file(pa_get_ps(pa), 0);
    }

    return 0;
}

// Internal service routine.
static void
_mon_process_ca(ca_o ca)
{
    CCS ofile;
    CS cabuf = NULL;

    // In the case of a recycled CA the pathcode will have been
    // determined during shopping.
    if (!ca_get_recycled(ca)) {
	ca_derive_pathcode(ca);
    }

    cabuf = (CS)ca_toCSVString(ca);

    if ((ofile = prop_get_str(P_OUTPUT_FILE))) {
	if (cabuf && *cabuf) {
	    FILE *fp;

	    fp = util_open_output_file(ofile);

	    if (fputs(cabuf, fp) == EOF || fputs("\n", fp) == EOF) {
		putil_syserr(0, "fputs(cabuf)");
	    }

	    fflush(fp);
	}
    }

    if (prop_has_value(P_SERVER) && !_mon_no_ptx()) {
	up_load_audit(cabuf);
	// Take each file marked for upload and tell libcurl to send it.
	(void)ca_foreach_cooked_pa(ca, _mon_process_pa, NULL);
    }

    putil_free(cabuf);

    if (prop_has_value(P_MAKE_DEPENDS) || prop_has_value(P_MAKE_FILE)) {
	make_file(ca);
    }

    if (prop_is_true(P_GIT)) {
	git_deliver(ca);
    }

    ca_set_processed(ca, 1);

    return;
}

// When an audit group has been destroyed, the references to CAs
// remain in the main table. This cleans them up.
static void
_mon_clean_up_ca_table(void)
{
    hnode_t *hnp;
    hscan_t hscan;

    hash_scan_begin(&hscan, AuditHash);
    for (hnp = hash_scan_next(&hscan); hnp; hnp = hash_scan_next(&hscan)) {
	ca_o ca;

	ca = (ca_o)hnode_get(hnp);
	if (ca_get_processed(ca)) {
	    ck_o ck;

	    ck = (ck_o)hnode_getkey(hnp);
	    hash_scan_delete(AuditHash, hnp);
	    hnode_destroy(hnp);
	    ck_destroy(ck);
	    ca_destroy(ca);
	}
    }
}

/// Called for each line received from the auditor. Note: the
/// passed-in string is mangled during parsing.
/// Parses it and adds it to the appropriate data structure.
/// @param[in] buf      a string in the canonical CSV format
/// @param[out] rcp     pointer to a place where a return code may be stored
/// @param[out] cmdpidp pointer to a place where the ending pid may be stored
/// @param[out] winner pointer to a place where the winning PTX may be stored
/// @return a bitmask describing what was learned from this line
unsigned
mon_record(CS buf, int *rcp, unsigned long *cmdpidp, CCS *winner)
{
    unsigned rc = 0;
    ck_o ck;
    ca_o ca;

    vb_printf(VB_MON, "=%s", buf);

    if (buf[0] == '<') {
	if (buf[1] == 'S' || buf[1] == 's') {		// SOA
	    int no_shop;
	    moment_s ca_start_time;
	    ca_o pred = NULL;
	    hnode_t *hnp;
	    CCS aggprop;
	    CCS cap;
	    agg_e agglevel = AGG_NEUTRAL;

	    rc = MON_SOA;

	    // The difference between 'S' and 's' above is that the
	    // lower-case version tells us not to bother shopping.
	    // Sometimes the auditor knows something we don't.
	    // Note that there's a matching 's' in the auditor!
	    no_shop = (buf[1] == 's') ||
		prop_is_true(P_UPLOAD_ONLY) ||
		prop_is_true(P_AUDIT_ONLY);

	    ca = ca_newFromCSVString(buf + strlen(SOA));
	    assert(ca);

	    ck = ck_new_from_ca(ca);
	    assert(ck);

	    // Record the starting time of this cmd.
	    moment_get_systime(&ca_start_time);
	    ca_set_starttime(ca, ca_start_time);

	    // Mark that it's started, even though it may not matter here.
	    ca_set_started(ca, 1);

	    if (hash_lookup(AuditHash, ck)) {
		putil_int("double delivery: '%s'", ca_toCSVString(ca));
	    }

	    // Find the immediate ancestor CA, if any. This should have
	    // a depth one less than current, possibly with the same
	    // pid (exec) or possibly with the parent pid (fork/exec).
	    if (ca_get_depth(ca)) {
		ck_o pck;

		pck = ck_new(ca_get_pccode(ca),
			     ca_get_depth(ca) - 1, ca_get_cmdid(ca));
		if ((hnp = hash_lookup(AuditHash, pck))) {
		    vb_printf(VB_MON, "exec parent match: %s", ca_get_prog(ca));
		    pred = (ca_o)hnode_get(hnp);
		} else {
		    ck_destroy(pck);
		    pck = ck_new(ca_get_pccode(ca),
				 ca_get_depth(ca) - 1, ca_get_pcmdid(ca));
		    if ((hnp = hash_lookup(AuditHash, pck))) {
			vb_printf(VB_MON, "fork parent match: %s",
				  ca_get_prog(ca));
			pred = (ca_o)hnode_get(hnp);
		    } else {
			ck_destroy(pck);
			pck = ck_new(ca_get_pccode(ca),
				     ca_get_depth(ca), ca_get_pcmdid(ca));
			if ((hnp = hash_lookup(AuditHash, pck))) {
			    pred = (ca_o)hnode_get(hnp);
			} else {
			    pred = NULL;
			}
		    }
		}
		ck_destroy(pck);
	    } else {
		pred = NULL;
	    }

	    // Put a pointer to this object in a table for quick lookup.
	    // Due to the possibility of CAs and PAs arriving out of sync
	    // we need a lookup to reassociate PAs with CAs at receive time.
	    if ((hnp = hnode_create(ca))) {
		hash_insert(AuditHash, hnp, ck);
	    } else {
		putil_syserr(2, "hnode_create()");
	    }

	    aggprop = prop_get_str(P_AGGREGATION_STYLE);

	    if (aggprop && *aggprop == '+') {
		// Aggregate everything into one big audit record.
		agglevel = AGG_STRONG;
	    } else if (aggprop && *aggprop == '-') {
		// Aggregate nothing; every cmd has its own audit record.
		agglevel = AGG_NONE;
	    } else {
		CCS prog, line;

		prog = ca_get_prog(ca);
		line = ca_get_line(ca);

		// Evaluate 'agglevel' based on whether this cmd
		// matches the current aggregation criteria. We allow cmds
		// to match on either the program name or the cmdline.
		// There are 3 aggregation types: weak, strong, and break.
		// The difference is that weak aggregations yield to
		// inner matches while strong aggregations are never
		// disbanded except in the case of an explicit break match.
		if (pred && ca_has_leader(pred) &&
		    ((cap = re_match__(prog_break_re, prog)) ||
		     (cap = re_match__(line_break_re, line)))) {
		    agglevel = AGG_BREAK;
		    vb_printf(VB_CA, "BREAK: [%s] %s", cap, line);
		} else if (pred && ca_has_leader(pred) &&
			   ca_get_strong(ca_get_leader(pred))) {
		    // No sense wasting time with REs here - continue strong.
		    agglevel = AGG_STRONG;
		} else if ((cap = re_match__(prog_strong_re, prog))) {
		    agglevel = AGG_STRONG;
		    vb_printf(VB_CA, "MATCH PROG STRONG: [%s] %s",
			      cap, line);
		} else if ((cap = re_match__(line_strong_re, line))) {
		    agglevel = AGG_STRONG;
		    vb_printf(VB_CA, "MATCH LINE STRONG: [%s] %s",
			      cap, line);
		} else if ((cap = re_match__(prog_weak_re, prog))) {
		    agglevel = AGG_WEAK;
		    vb_printf(VB_CA, "MATCH PROG WEAK: [%s] %s", cap, line);
		} else if ((cap = re_match__(line_weak_re, line))) {
		    agglevel = AGG_WEAK;
		    vb_printf(VB_CA, "MATCH LINE WEAK: [%s] %s", cap, line);
		} else {
		    agglevel = AGG_NEUTRAL;
		    vb_printf(VB_CA, "NO MATCH: %s", line);
		}
	    }

	    // Now we know if this CA is aggregating and how strong it is.
	    if (pred && ca_has_leader(pred)) {
		if (agglevel != AGG_BREAK && ca_get_strong(ca_get_leader(pred))) {
		    // If the current group is strong, it always wins.
		    _mon_verbosity(ca, "PUSH STRONG");
		    ca_aggregate(ca_get_leader(pred), ca);
		} else if (agglevel == AGG_BREAK ||
			   agglevel == AGG_STRONG || agglevel == AGG_WEAK) {
		    // If already within a "weak" audit group, we must
		    // disband the existing group by sending each of
		    // its audits to the server individually.
		    ca_disband(ca_get_leader(pred), _mon_process_ca);

		    if (agglevel != AGG_BREAK) {
			// Start a new group with ourself as the leader.
			ca_start_group(ca, agglevel == AGG_STRONG);
		    }
		    // At this point the AG has been destroyed and its
		    // CAs are no longer valid. However, we must still
		    // remove references to them from the main table.
		    _mon_clean_up_ca_table();
		} else if (agglevel == AGG_NEUTRAL) {
		    _mon_verbosity(ca, "PUSH WEAK");
		    ca_aggregate(ca_get_leader(pred), ca);
		} else {
		    putil_int("impossible agglevel: '%s'",
			      ca_toCSVString(ca));
		}
	    } else if (agglevel == AGG_NONE ||
		       agglevel == AGG_NEUTRAL || agglevel == AGG_BREAK) {
		_mon_verbosity(ca, "SINGULAR");
	    } else if (agglevel == AGG_STRONG || agglevel == AGG_WEAK) {
		// Start a new group with ourself as the leader.
		_mon_verbosity(ca, "PUSH NEW");
		ca_start_group(ca, agglevel == AGG_STRONG);
	    } else {
		putil_int("impossible agg state: '%s'", ca_toCSVString(ca));
	    }

	    if (ca_is_top(ca)) {
		vb_printf(VB_MON, "START: %lu", ca_get_cmdid(ca));
		rc |= MON_TOP;
	    }

	    // Here is where recycling takes place. If shopping was
	    // successful, tell the command it's done. If not, let
	    // it run unless we're in strict mode.
	    if (prop_has_value(P_SERVER) && !no_shop) {
		shop_e shoprc;

		shoprc = shop(ca, NULL, 1);

		if (shoprc == SHOP_RECYCLED) {
		    rc |= MON_RECYCLED;
		    if (winner) {
			*winner = ca_get_recycled(ca);
		    }
		} else if (shoprc == SHOP_MUSTRUN ||
			   shoprc == SHOP_MUSTRUN_AGG) {
		    // A "must run" (no download available) condition.
		    if (shoprc == SHOP_MUSTRUN_AGG) {
			// Report that nested aggegated cmds can suppress
			// shopping since they're guaranteed not to match.
			rc |= MON_AGG;
		    }

		} else if (shoprc == SHOP_OFF) {
		    // We've deliberately suppressed shopping.
		} else if (shoprc == SHOP_NOMATCH ||
			   shoprc == SHOP_NOMATCH_AGG ||
			   shoprc == SHOP_ERR) {
		    // We tried to find a match but failed. The user
		    // can ask for this to be treated as an error.
		    if (prop_is_true(P_STRICT_DOWNLOAD)) {
			putil_error("Failed %s requirement on '%s'",
			    prop_to_name(P_STRICT_DOWNLOAD),
			    ca_get_line(ca));
			rc |= MON_STRICT;
		    }
		    if (shoprc == SHOP_NOMATCH_AGG) {
			// Report that nested aggegated cmds can suppress
			// shopping since they're guaranteed not to match.
			rc |= MON_AGG;
		    }
		} else {
		    putil_int("unknown shopping result: %d", (int)shoprc);
		}
	    }
	} else if (buf[1] == 'E') {			// EOA
	    moment_s ca_end_time;
	    CS csv;
	    int cmdrc;
	    unsigned long cmdid;
	    hnode_t *hnp, *next;
	    ca_o eoa, ldr, curr;
	    int ended = 0;

	    rc = MON_EOA;

	    // Record the ending time right away while it's most accurate.
	    moment_get_systime(&ca_end_time);

	    // Skip past the EOA marker.
	    csv = buf + strlen(EOA);

	    // If a return code is encoded, report and move past it.
	    if (*csv == '[') {
		csv++;
		cmdrc = atoi(csv);
		csv = strchr(csv, ']');
		csv++;
	    } else {
		cmdrc = 0;
	    }

	    // Kind of indirect here ... we create a CA object from
	    // the EOA line which should be functionally identical to
	    // the one from the SOA line. We use the EOA version to
	    // look up the SOA version (which must already be present),
	    // then throw away the EOA version to avoid a memory leak.
	    eoa = ca_newFromCSVString(csv);

	    // The lookup key associated with the EOA CA.
	    ck = ck_new_from_ca(eoa);

	    // Retrieve the "real" CA object with this signature.
	    if (!AuditHash || !(hnp = hash_lookup(AuditHash, ck))) {
		CCS str;

		// Whoops.
		str = ca_toCSVString(eoa);
		putil_warn("EOA skew: %s", str);
		putil_free(str);
		mon_dump();
		ca_destroy(eoa);
		ck_destroy(ck);
		return MON_ERR;
	    }

	    // Retrieve the original CA for which we just received EOA.
	    // Note that 'hnp' remains valid and is used below.
	    ca = (ca_o)hnode_get(hnp);

	    // Don't need these any more.
	    ck_destroy(ck);
	    ca_destroy(eoa);

	    // N.B. - it's critical to store these attributes *now*
	    // because 'ca' may get destroyed (freed) in the upcoming loop.
	    cmdid = ca_get_cmdid(ca);
	    ldr = ca_get_leader(ca);

	    // At least on Unix, a process can be overlaid with a
	    // new image via exec(). In this case we might see a
	    // chain of cmds sharing one pid. Only the last one will
	    // send an EOA because EOA is sent only at exit and each
	    // process can exit only once. Therefore, upon receiving
	    // an EOA we must always "swim upstream" to close all
	    // predecessor audits sharing the same cmdid.

	    // Note - this depends on 'hnp' still being set from above.
	    for (; hnp; hnp = next) {
		unsigned long duration;

		// Grab a pointer to each CA working up the chain.
		// The first time through this will be the same CA
		// we were already working on.
		curr = (ca_o)hnode_get(hnp);

		if (ca_get_cmdid(curr) != cmdid) {
		    break;
		}

		// Calculate the elapsed time between cmd start and end.
		// This is tricky because, in a chain of execs with no
		// intervening forks, we have no record of the ending
		// time of any command but the last. HOWEVER, logic
		// tells us that the ending time of a command calling
		// exec is the starting time of the exec-ed command,
		// so after calculating the duration of each CA in this
		// chain we reset the ending time to its start time.
		duration = moment_duration(ca_end_time, ca_get_starttime(curr));
		ca_set_duration(curr, duration);
		ca_end_time = ca_get_starttime(curr);

		// If we've reached the end of the top of the cmd tree,
		// the entire build must be done. The EOA of the top-level
		// cmd is required to arrive last.
		if (ca_is_top(curr)) {
		    ended = 1;
		}

		if (ca_get_depth(curr) > 0) {
		    ck_o tck;

		    // Make a temporary key to search for this CA's parent,
		    // Grab a pointer to the predecessor, if any, before
		    // proceeding, and delete the temporary search key.
		    tck = ck_new(ca_get_pccode(curr),
				 ca_get_depth(curr) - 1, cmdid);
		    next = hash_lookup(AuditHash, tck);

		    ck_destroy(tck);
		} else {
		    // We must be at the very top of the cmd tree.
		    next = NULL;
		}

		// Now we can close the current CA.
		if (ca_has_leader(curr)) {
		    _mon_verbosity(curr, "CLOSE");
		    ca_set_closed(curr, 1);
		} else {
		    // If not in a group, finish it off directly.
		    ca_set_closed(curr, 1);
		    ca_publish(curr, _mon_process_ca);
		    _mon_clean_up_ca_table();
		}
	    }

	    // If this audit is a member of an audit group and the entire
	    // group is ready to go, send it.
	    if (ldr && ca_get_closed(ldr)) {
		if (ca_get_pending(ldr)) {
		    putil_warn("audit group closed with %d pending audits",
			       ca_get_pending(ldr));
		}

		ca_publish(ldr, _mon_process_ca);

		// At this point the AG has been destroyed and its
		// CAs are no longer valid. However, we must still
		// remove references to them from the main table.
		_mon_clean_up_ca_table();
	    }

	    if (ended) {
		rc |= MON_TOP;
		vb_printf(VB_MON, "END: %lu", cmdid);

		if (cmdpidp) {
		    *cmdpidp = cmdid;
		}
		if (rcp && *rcp == 0) {
		    *rcp = cmdrc;
		}
	    }
	} else {
	    putil_warn("unrecognized line: %s", buf);
	    rc = MON_ERR;
	}
    } else if (ISALPHA(buf[0])) {
	CCS path;
	pa_o pa;
	hnode_t *hnp;

	rc = MON_NEXT;

	// De-serialize the PA line into an object.
	pa = pa_newFromCSVString(buf);
	assert(pa);

	path = pa_get_abs(pa);

	// Any file that's written should be marked for upload.
	// And optionally files which are only read too.
	if (pa_is_write(pa) || prop_is_true(P_UPLOAD_READS)) {
	    pa_set_uploadable(pa, 1);
	}

	// Need a key object for looking up the associated CA.
	ck = ck_new(pa_get_ccode(pa), pa_get_depth(pa), pa_get_pid(pa));
	assert(ck);

	// If we can't find the associated CK, something's wrong.
	if (!AuditHash || !(hnp = hash_lookup(AuditHash, ck))) {
	    CCS str;

	    str = pa_tostring(pa);
	    putil_warn("PA skew [%s]", str);
	    putil_free(str);
	    mon_dump();
	    ck_destroy(ck);
	    pa_destroy(pa);
	    return MON_ERR;
	}

	ck_destroy(ck);

	// This is the CA we needed.
	ca = (ca_o)hnode_get(hnp);

	ca_record_pa(ca, pa);
    } else if (buf[0] == '+') {
	// Verbosity from auditor. Just print it.
	rc = MON_NEXT;
	fputs(buf, stderr);
	fputc('\n', stderr);
    } else if (buf[0] == '#') {
	// Comment from auditor. Ignore it.
	rc = MON_NEXT;
    } else if (buf[0] == '!') {
	// Special message from auditor - shut down immediately.
	// This is used when the top-level process can't run at all.
	rc = MON_CANTRUN;
    } else {
	putil_warn("unrecognized line: %s", buf);
	rc = MON_ERR;
    }

    return rc;
}

/// Communicate with the server to establish a session.
/// @return nonzero on failure
int
mon_begin_session(void)
{
    char *url;
    CURL *curl;
    int rc = 0;

    if (_mon_no_ptx()) {
	return rc;
    }

    url = http_make_url(SESSION_SERVLET_NICKNAME);

    http_add_param(&url, HTTP_PROJECT_NAME_PARAM, prop_get_str(P_PROJECT_NAME));
    http_add_param(&url, HTTP_SESSION_TIMEOUT_SECS_PARAM,
		   prop_get_str(P_SESSION_TIMEOUT_SECS));

    // If this property is set it will override the default logging
    // level over on the server side. The values are the public
    // fields of org.apache.log4j.Level in string form.
    http_add_param(&url, HTTP_LOG_LEVEL_PARAM,
		   prop_get_str(P_SERVER_LOG_LEVEL));

    curl = http_get_curl_handle(1);

    // This is where we pick up special property-setting headers ...
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, http_parse_startup_headers);

    // Make the connection to start a new HTTP session ...
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rc);
    if (http_connect(curl, url, 0)) {
	rc = 1;
    }

    return rc;
}

/// Communicate with the server to establish a new PTX.
/// @return nonzero on failure
void
mon_ptx_start(void)
{
    char *url;
    struct utsname sysdata;
    CCS rwd;
    CURL *curl;
    int rc = 0;

    if (!prop_has_value(P_SERVER)) {
	return;
    }

    if (_mon_no_ptx()) {
	return;
    }

    url = http_make_url(START_SERVLET_NICKNAME);

    http_add_param(&url, HTTP_PROJECT_NAME_PARAM, prop_get_str(P_PROJECT_NAME));
    http_add_param(&url, HTTP_BASE_DIR_PARAM, prop_get_str(P_BASE_DIR));
    http_add_param(&url, HTTP_LOGIN_NAME_PARAM, util_get_logname());
    http_add_param(&url, HTTP_GROUP_NAME_PARAM, util_get_groupname());
    http_add_param(&url, HTTP_RWD_PARAM, rwd = util_get_rwd());
    putil_free(rwd);
    if (putil_uname(&sysdata) != -1) {
	http_add_param(&url, HTTP_SYSTEM_NAME_PARAM, sysdata.sysname);
	http_add_param(&url, HTTP_HOST_NAME_PARAM, sysdata.nodename);
	http_add_param(&url, HTTP_OS_RELEASE_PARAM, sysdata.release);
	http_add_param(&url, HTTP_MACHINE_TYPE_PARAM, sysdata.machine);
    } else {
	putil_syserr(2, "uname");
    }

    // Provide the starting time from the client's POV. The server
    // can determine start time on its own but clock skew may exist
    // between client and server.
    {
	moment_s ptx_start_time;
	char startbuf[MOMENT_BUFMAX];

	moment_get_systime(&ptx_start_time);
	(void)moment_format(ptx_start_time, startbuf, charlen(startbuf));
	http_add_param(&url, HTTP_CLIENT_START_TIME_PARAM, startbuf);
    }

    // If there's no way we're going to be contributing anything,
    // the server needs to know that. This flag tells the server not
    // to make any changes to the persistent state; thus it would not
    // be appropriate for Audit.Only, for instance.
    if (prop_is_true(P_DOWNLOAD_ONLY)) {
	http_add_param(&url, HTTP_READ_ONLY_PARAM, HTTP_TRUE);
    }

    curl = http_get_curl_handle(1);

    // This is where we pick up special property-setting headers ...
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, http_parse_startup_headers);

    // Normally, reusing the curl connection is a good thing. However,
    // since this is the last control connection until the build
    // finishes, the socket could be held open for a very long time.
    // I've been told there's no big harm in that but at the least
    // it would leave one less file descriptor free for other tasks,
    // and there's no great downside to re-establishing the socket
    // at finish-up time either, so we turn off socket reuse for this
    // connection.
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);

    // Make the connection to start a new PTX ...
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rc);
    if (http_connect(curl, url, 0)) {
	rc = 1;
    }

    if (rc) {
	putil_die("can't find a server at %s", prop_get_str(P_SERVER));
    }

    // Initialize the uploader module.
    up_init();
}

/// Communicate with the server to get a roadmap for the current project.
void
mon_get_roadmap(void)
{
    char *url;
    struct utsname sysdata;
    CURL *curl;
    CCS rmap;
    FILE *fp;

    url = http_make_url(ROADMAP_SERVLET_NICKNAME);

    http_add_param(&url, HTTP_PROJECT_NAME_PARAM, prop_get_str(P_PROJECT_NAME));

    // Optional override of PTX selection criteria.
    http_add_param(&url, HTTP_PTX_STRATEGY_PARAM, prop_get_str(P_PTX_STRATEGY));

    http_add_param(&url, HTTP_LOGIN_NAME_PARAM, util_get_logname());
    http_add_param(&url, HTTP_GROUP_NAME_PARAM, util_get_groupname());

    if (putil_uname(&sysdata) != -1) {
	http_add_param(&url, HTTP_HOST_NAME_PARAM, sysdata.nodename);
    }

    // Tell the server we want to see only members in
    // the roadmap. Currently, this boolean is handled server
    // side while the RE-based filtering is done client side.
    // This allows optional warnings to be issued for
    // non-matches, at the cost of bigger roadmaps and
    // unnecessary comparisons. Subject to redesign.
    if (prop_is_true(P_MEMBERS_ONLY)) {
	http_add_param(&url, HTTP_SHOP_MEMBERS_ONLY_PARAM, HTTP_TRUE);
    }

    curl = http_get_curl_handle(1);

    prop_assert(P_ROADMAPFILE);
    rmap = prop_get_str(P_ROADMAPFILE);
    vb_printf(VB_SHOP, "GETTING ROADMAP FILE %s", rmap);

    if ((fp = fopen(rmap, "wb"))) {
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	if (http_connect(curl, url, 0)) {
	    putil_exit(2);
	}

	if (fclose(fp)) {
	    putil_syserr(2, rmap);
	}
    } else {
	putil_syserr(2, rmap);
    }
}

/// Communicate with the server to close the session for this PTX.
/// A logfile may optionally be uploaded at the same time.
/// @param[in] rc       the return code of the audited command
/// @param[in] logfile  the name of a logfile to upload, or NULL
void
mon_ptx_end(int rc, CCS logfile)
{
    char *url;
    CURL *curl;
    char numbuf[32];

    if (!prop_has_value(P_SESSIONID)) {
	return;
    }

    // Make sure all async libcurl transfer are completed.
    http_async_transfer(1);

    if (_mon_no_ptx()) {
	return;
    }

    if (logfile) {
	ps_o ps;

	ps = ps_newFromPath(logfile);
	up_load_file(ps, 1);
	ps_destroy(ps);
    }

    // Finalize upload stuff.
    up_fini();

    // Make the connection to end the PTX ...
    url = http_make_url(END_SERVLET_NICKNAME);

    if (prop_is_true(P_AGGRESSIVE_SERVER)) {
	http_add_param(&url, HTTP_AGGRESSIVE_PARAM, "1");
    }

    curl = http_get_curl_handle(1);

    // Indicate whether the PTX provided a successful return code.
    snprintf(numbuf, charlen(numbuf), "%d", rc);
    http_add_header(curl, X_CLIENT_STATUS_HEADER, numbuf);

    snprintf(numbuf, charlen(numbuf), "%d", shop_get_count());
    http_add_header(curl, X_RECYCLED_COUNT_HEADER, numbuf);

    (void)http_connect(curl, url, 0);

    // If there's a chance of another cycle, refresh the roadmap
    // to include the PTX just finished.
    if (prop_has_value(P_ACTIVATION_PROG_RE) &&
	    !prop_is_true(P_REUSE_ROADMAP)) {
	(void)mon_get_roadmap();
    }
}

/// Communicate with the server to end the session, and finalize
/// some monitor data structures.
void
mon_fini(void)
{
    // Free up all compiled regular expressions.
    re_fini__(&line_break_re);
    re_fini__(&prog_break_re);
    re_fini__(&line_strong_re);
    re_fini__(&prog_strong_re);
    re_fini__(&line_weak_re);
    re_fini__(&prog_weak_re);

    if (AuditHash) {
	if (hash_count(AuditHash)) {
	    putil_warn("%d audits left over:",
		(int)hash_count(AuditHash));
	    mon_dump();
	}
	hash_destroy(AuditHash);
	AuditHash = NULL;
    }

    // Finalize dcode cache management if in use.
    ps_dcode_cache_fini();

    shop_fini();
}
