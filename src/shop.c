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
/// @brief Central logic for roadmap parsing and shopping-related activities.

#include "AO.h"

#include "CA.h"
#include "DOWN.h"
#include "PA.h"
#include "PS.h"
#include "PROP.h"
#include "RE.h"
#include "SHOP.h"

#include "cdb.h"
#include "dict.h"

#if !defined(_WIN32)
#include <sys/mman.h>
#if  !defined(BSD)
#include <alloca.h>
#endif	/*BSD*/
#endif	/*_WIN32*/

/// @cond static
#define PTX_PREFIX		"X"

#define RMAP_RADIX		36

#define IS_TRUE(expr)		(!stricmp((expr), "true"))

typedef int (*tgt_process_f) (pa_o, void *);

/// @endcond static

/// Structure type used to carry shopping state through potential recursions.
typedef struct {
    struct cdb *cdbp;		///< Holds current CDB state
    ca_o ca;			///< Describes the cmd we want to match
    int getfiles;		///< Boolean - really get files?
    dict_t *ptx_dict;		///< Holds PTX table
    void *ignore_path_re;	///< RE describing paths to skip
    char winner[32];		///< Will hold the winning PTX id
    char wix[32];		///< Will hold the winning PTX index
    char wincmd[32];		///< Will hold the winning cmd index
} shopping_state_s;

static int RecycledCount;

static struct cdb *ShopCDB;

/// Initialize all PTX-related data structures.
static dict_t *
_shop_ptx_init(void)
{
    dict_t *dict;

    // Note that we look up keys in a case-insensitive manner
    // because we play games with the case.
    if (!(dict = dict_create(DICTCOUNT_T_MAX, (dict_comp_t)stricmp))) {
	putil_syserr(2, "dict_create(ptx_dict)");
    }
    return dict;
}

/// Return nonzero iff the specified key is present.
static int
_shop_ptx_contains(shopping_state_s *ssp, CCS key)
{
    return dict_lookup(ssp->ptx_dict, key) != NULL;
}

// Mark a PTX as having been evaluated by converting its index
// to lower case (lookups are case-insensitive so this is a no-op
// from the POV of the table).
static void
_shop_ptx_mark_as_seen(shopping_state_s *ssp, CCS key) {
    dnode_t *dnp;
    CS realkey;

    if ((dnp = dict_lookup(ssp->ptx_dict, key))) {
	realkey = (CS)dnode_getkey(dnp);
	if (isupper((int)realkey[0])) {
	    realkey[0] = tolower((int)realkey[0]);
	}
    }
}

/// Store a new PTX id in the internal database.
static void
_shop_ptx_insert(shopping_state_s *ssp, CCS key, CCS id)
{
    if (!dict_lookup(ssp->ptx_dict, key)) {
	if (!dict_alloc_insert(ssp->ptx_dict,
			       putil_strdup(key), putil_strdup(id))) {
	    putil_syserr(2, "dict_alloc_insert(ptx_dict)");
	}
    }
}

/// Remove the specified PTX from the DB via its index.
/// @param[in] key      the key associated with a given PTX
/// return zero iff the node was found and successfully removed
static int
_shop_ptx_invalidate(shopping_state_s *ssp, CCS key, CCS msg, int ignored)
{
    dnode_t *dnp;
    CCS id;

    dnp = dict_lookup(ssp->ptx_dict, key);

    if (ignored && dnp) {
	id = (CCS)dnode_get(dnp);
	vb_printf(VB_WHY, "WOULD INVALIDATE %s (%s) due to '%s'", id, key, msg);
	return 0;
    }

    if (dnp) {
	id = (CCS)dnode_get(dnp);
	key = (CCS)dnode_getkey(dnp);
	vb_printf(VB_WHY, "PTX %s invalidated due to '%s'", id, msg);
	dict_delete_free(ssp->ptx_dict, dnp);
	putil_free(id);
	putil_free(key);
	return 0;
    } else {
	putil_warn("invalidated PTX %s twice", key);
	return 1;
    }
}

/// Used to find out how many, if any, PTXes remain viable.
/// @return the number of PTXes in the internal database
static long
_shop_ptx_count(shopping_state_s *ssp)
{
    long count;

    count = ssp->ptx_dict ? (long)dict_count(ssp->ptx_dict) : 0;
    return count;
}

/// Any PTXes which survived shopping are eligible to be winners,
/// as long as they've been seen and evaluated at all. This test is
/// necessary because not all commands are run in all PTXes, so the
/// fact that a PTX has not been invalidated could mean it was not
/// even evaluated.
/// This function returns the first eligible surviving PTX. It does not
/// establish a policy per se; since PTXes are stored in an order set by
/// the server, the policy of which matching PTX is 'best' can be
/// dictated by the server.
/// @return the chosen PTX id
static CCS
_shop_ptx_winner(shopping_state_s *ssp)
{
    dnode_t *dnp, *next;

    ssp->wix[0] = ssp->winner[0] = '\0';

    if (ssp->ptx_dict) {
	for (dnp = dict_first(ssp->ptx_dict); dnp;) {
	    CCS key, id;

	    next = dict_next(ssp->ptx_dict, dnp);

	    key = (CCS)dnode_getkey(dnp);
	    id = (CCS)dnode_get(dnp);

	    // Remember, only an evaluated PTX can be a winner.
	    // These are marked with a lower-case index, but
	    // convert it back to upper case before returning.
	    if (islower((int)key[0])) {
		snprintf(ssp->wix, charlen(ssp->wix), "%c%s",
		    toupper((int)key[0]), key + 1);
		snprintf(ssp->winner, charlen(ssp->winner), "%s", id);
		return ssp->winner;
	    }

	    dnp = next;
	}
    }

    return NULL;
}

/// Free all data structures allocated by _shop_ptx_init().
static void
_shop_ptx_destroy(dict_t *dict)
{
    dnode_t *dnp, *next;

    if (dict) {
	for (dnp = dict_first(dict); dnp;) {
	    CCS key, id;

	    next = dict_next(dict, dnp);

	    key = (CCS)dnode_getkey(dnp);
	    id = (CCS)dnode_get(dnp);
	    dict_delete_free(dict, dnp);
	    putil_free(key);
	    putil_free(id);
	    dnp = next;
	}
	dict_destroy(dict);
    }
}

// This is going to be slow since it is a search by value. I.e. it
// walks through the entire CDB sequentially trying to map a value
// back to its key. Therefore it should only be used in debug or
// error scenarios.
static CCS
_shop_find_cmdline(shopping_state_s *ssp, CCS cmdix)
{
    unsigned loc = 0;

    cdb_seqinit(&loc, ssp->cdbp);
    while (cdb_seqnext(&loc, ssp->cdbp) > 0) {
	unsigned len;

	CS cmdstate, ix;

	len = cdb_datalen(ssp->cdbp);
	cmdstate = (CS)alloca(len + 1);
	cdb_read(ssp->cdbp, cmdstate, len, cdb_datapos(ssp->cdbp));
	cmdstate[len] = '\0';

	if ((ix = util_strsep(&cmdstate, FS1)) && !strcmp(cmdix, ix)) {
	    CS line;

	    len = cdb_keylen(ssp->cdbp);
	    line = (CS)putil_malloc(len + 1);
	    cdb_read(ssp->cdbp, line, len, cdb_keypos(ssp->cdbp));
	    line[len] = '\0';
	    return line;
	}
    }

    return NULL;
}

/// @param[in] spa              the PA holding the target path state
/// @param[in] data             a pointer to the shopping state structure
/// @return -1 on error, 0 if no download required, 1 if download required
static int
_shop_process_target(pa_o spa, void *data)
{
    int rc = 0;
    shopping_state_s *ssp;
    CCS path;

    ssp = (shopping_state_s *) data;

    path = pa_get_abs(spa);

    if (!pa_get_uploadable(spa)) {
	return 0;
    } else if (pa_is_unlink(spa)) {
	if (pa_exists(spa)) {
	    vb_printf(VB_STD, "UNLINKING %s", pa_get_rel(spa));
	}
    } else if (pa_is_link(spa)) {
	vb_printf(VB_STD, "LINKING %s -> %s",
		  pa_get_rel(spa), pa_get_rel2(spa));
    } else if (pa_is_symlink(spa)) {
	vb_printf(VB_STD, "SYMLINKING %s -> %s",
		  pa_get_rel(spa), pa_get_target(spa));
    } else {
	// If the file exists locally, make a client-side PS object
	// representing it and compare it vs the server-side PS.
	// If identical we can skip the download.
	if (pa_exists(spa)) {
	    ps_o cps;

	    int avoidable;

	    cps = ps_newFromPath(path);
	    if ((avoidable = !ps_stat(cps, pa_has_dcode(spa)))) {
		avoidable = !ps_diff(pa_get_ps(spa), cps);
	    }
	    ps_destroy(cps);
	    if (avoidable) {
		pn_verbosity(ps_get_pn(pa_get_ps(spa)),
		    "REUSING", ssp->winner);
		if (ssp->getfiles) {
		    RecycledCount++;
		    if (!prop_is_true(P_ORIGINAL_DATESTAMP)) {
			if (moment_set_mtime(NULL, path)) {
			    putil_syserr(0, path);
			}
		    }
		}
		return 0;
	    }
	}
    }

    if (ssp->getfiles) {
	ps_o sps;
	char *linkdir = NULL;

	sps = pa_get_ps(spa);

	if (pa_is_unlink(spa)) {
	    if (pa_exists(spa) && unlink(path)) {
		putil_syserr(0, path);
		rc = -1;
	    }
#if !defined(_WIN32)
	} else if (pa_is_link(spa)) {
	    CCS path2;

	    path2 = pa_get_abs2(spa);

	    if ((linkdir = putil_dirname(path)) && access(linkdir, F_OK)) {
		// If the parent dir of the link doesn't exist, make it.
		if (putil_mkdir_p(linkdir)) {
		    putil_syserr(0, linkdir);
		}
	    } else if (!unlink(path)) {
		// If the link already exists: it would be way too much work
		// to try and figure out whether it's linked to the right
		// file(s), so instead we just unlink and relink. Which
		// has the unfortunate side effect of changing the
		// timestamp on the directory but that dir is most likely
		// being written to anyway by other build artifacts.
		if (errno != ENOENT) {
		    putil_syserr(0, path);
		}
	    }
	    putil_free(linkdir);
	    if (link(path2, path)) {
		putil_lnkerr(0, path2, path);
		rc = -1;
	    }
	} else if (pa_is_symlink(spa)) {
	    CCS lbuf;
	    CCS target;

	    target = pa_get_target(spa);

	    // If the target exists and is already a symlink, compare
	    // it for equality. Better not to recreate it unnecessarily
	    // since that would affect its timestamp and ownership.
	    // These may be only cosmetic for symlinks in most cases but
	    // looks matter too.
	    if ((lbuf = putil_readlink(path))) {
		if (strcmp(target, lbuf)) {
		    // TODO: COVERITY TOCTOU
		    if (!access(path, F_OK) && unlink(path)) {
			putil_syserr(0, path);
			rc = -1;
		    } else {
			if (symlink(target, path)) {
			    putil_lnkerr(0, target, path);
			    rc = -1;
			}
		    }
		}
		putil_free(lbuf);
	    } else {
		if (errno != ENOENT) {
		    if (unlink(path)) {
			putil_syserr(0, path);
		    }
		}
		// In case the parent dir of the symlink doesn't exist, make it.
		if ((linkdir = putil_dirname(path))) {
		    if (access(linkdir, F_OK)) {
			if (putil_mkdir_p(linkdir)) {
			    putil_syserr(0, linkdir);
			}
		    }
		    putil_free(linkdir);
		}
		if (symlink(target, path)) {
		    putil_lnkerr(0, target, path);
		    rc = -1;
		}
	    }
#endif				/*!_WIN32 */
	} else if (pa_is_dir(spa)) {
	    // TODO: COVERITY TOCTOU
	    if (access(path, F_OK)) {
		if (putil_mkdir_p(path)) {
		    putil_syserr(0, path);
		} else {
#if !defined(_WIN32)
		    if (chmod(path, ps_get_mode(sps))) {
			putil_syserr(0, path);
		    }
#endif /*!_WIN32 */
		}
	    }
	} else {
	    // We have a live one! For symmetry with uploads we do not
	    // report the download of zero-length files even though they
	    // may require a servlet invocation to get mode and datestamp.
	    if (ps_get_size(sps) > 0) {
		pn_verbosity(ps_get_pn(sps), "DOWNLOADING", ssp->winner);
	    }
	    rc = down_load(sps);
	    if (rc == 0 && ps_get_size(sps) > 0) {
		RecycledCount++;
	    }
	}

	// Mark this PA as already processed.
	if (rc == 0) {
	    pa_set_uploadable(spa, 0);
	} else {
	    if (prop_is_true(P_STRICT_DOWNLOAD)) {
		putil_exit(2);
	    }
	}
    }

    return rc;
}

// We may compare any number of path *states* against
// the same path *name*. The current state will not change.
// Therefore, in order to prevent redundant checksumming,
// we try to save the current state until we've moved on
// to a new pathname. The current implementation relies on
// two assumptions: first that CDB returns keys in
// the same order they were inserted (which appears to be
// the case but is nowhere promised), and second that the
// server sends all states associated with a given path name
// together, which it currently does. If either of these breaks
// down the code will still work but sub-optimally.
static ps_o CurrentPS;

static void
_shop_cmp_pathstate(shopping_state_s *ssp, CCS pskey, CS ptxes1)
{
    unsigned vlen;
    int ignored;
    pa_o dummy_pa;
    ps_o shopped_ps;
    CCS path, reason = NULL;
    CS csv, ptxbuf, explanation = NULL;

    if (cdb_find(ssp->cdbp, pskey, strlen(pskey)) <= 0) {
	putil_int("bad PS key in roadmap: %s", pskey);
	return;
    }

    vlen = cdb_datalen(ssp->cdbp);
    csv = (CS)alloca(vlen + 1);
    cdb_read(ssp->cdbp, csv, vlen, cdb_datapos(ssp->cdbp));
    csv[vlen] = '\0';
    shopped_ps = ps_newFromCSVString(csv);

    path = ps_get_abs(shopped_ps);

    // Figure out if this path is to be ignored per user request.
    // We may still want to do the comparison for ignored paths
    // in order to issue warnings (the user might wish to know
    // what didn't match without failing the build).
    ignored = re_match__(ssp->ignore_path_re, path) != NULL;

    if (!CurrentPS || strcmp(ps_get_abs(CurrentPS), path)) {
	ps_destroy(CurrentPS);
	CurrentPS = ps_newFromPath(path);
	if (ps_stat(CurrentPS, ps_has_dcode(shopped_ps))) {
	    asprintf(&explanation, "%s: %s", path, strerror(errno));
	}
    }

    // Do the actual comparison.
    // IDEA - choose a model here: Either we can ignore paths
    // before comparison (thus saving time) or after (allowing
    // a useful warning to be issued). Or we could add a new
    // preference allowing user choice.
    if (explanation || (reason = ps_diff(shopped_ps, CurrentPS))) {
	CS ixstr;

	// If this iteration produced a mismatch, invalidate all the
	// PTXes associated with that PS.
	if (reason) {
	    if (asprintf(&explanation, "%s mismatch on %s",
		    reason, path) < 0) {
		putil_syserr(2, NULL);
	    }
	}

	ptxbuf = (CS)alloca(strlen(ptxes1) + CHARSIZE);
	strcpy(ptxbuf, ptxes1);
	for (ixstr = util_strsep(&ptxbuf, FS2);
		 ixstr && _shop_ptx_count(ssp);
		 ixstr = util_strsep(&ptxbuf, FS2)) {
	    _shop_ptx_invalidate(ssp, ixstr, explanation, ignored);
	}

	// If the failing path is not ignored for shopping, this
	// loop iteration is finished.
	if (!ignored) {
	    ps_destroy(shopped_ps);
	    return;
	}
    }

    putil_free(explanation);

    // The current PS matched, so hold onto it. Unfortunately a
    // CA object doesn't hold PS objects, it holds PAs, so we
    // must wrap the PS in a dummy PA.
    // The only reason prereqs are saved at all is to let a
    // recycled CA generate the same signature as if it had
    // actually run.
    dummy_pa = pa_new();
    pa_set_ps(dummy_pa, ps_copy(CurrentPS));
    pa_set_op(dummy_pa, OP_READ);
    pa_set_call(dummy_pa, "dummy");
    ca_record_pa(ssp->ca, dummy_pa);

    ps_destroy(shopped_ps);
    return;
}

static void
_shop_compare_prereqs(shopping_state_s *ssp, CCS cmdix)
{
    char key[64];
    struct cdb_find cdbf_prq;
    unsigned vlen;

    snprintf(key, charlen(key), "<%s", cmdix);
    if (cdb_findinit(&cdbf_prq, ssp->cdbp, key, strlen(key)) < 0) {
	putil_die("cdb_findinit: %s (%s)", key, ca_get_line(ssp->ca));
    }

    while (cdb_findnext(&cdbf_prq) > 0 && _shop_ptx_count(ssp)) {
	CCS pskey;
	CS pskeys, prqline, ptxes1, ptxbuf, ixstr;
	int ptxesleft;

	// Read a prereq line from the roadmap.
	vlen = cdb_datalen(ssp->cdbp);
	prqline = (CS)alloca(vlen + 1);
	cdb_read(ssp->cdbp, prqline, vlen, cdb_datapos(ssp->cdbp));
	prqline[vlen] = '\0';

	// Split the line into a list of path states and a list of ptxes.
	if (!(ptxes1 = strstr(prqline, FS1))) {
	    putil_int("bad format in roadmap: %s", prqline);
	    break;
	}
	*ptxes1++ = '\0';
	pskeys = prqline;

	// Here we make a temporary copy of the ptx list and take a
	// quick run through it. If none of the listed ptxes remain
	// eligible, we can skip evaluating this set of path states.
	// On the other hand, if we do go on to evaluate any of these
	// ptxes, mark them as having been evaluated. In order to
	// be a winner you must not only survive the war but also
	// show evidence of having fought. No one gets a medal for
	// hiding in a foxhole, so to speak.
	ptxbuf = (CS)alloca(strlen(ptxes1) + CHARSIZE);
	strcpy(ptxbuf, ptxes1);
	for (ptxesleft = 0, ixstr = util_strsep(&ptxbuf, FS2);
		 ixstr;
		 ixstr = util_strsep(&ptxbuf, FS2)) {
	    if (_shop_ptx_contains(ssp, ixstr)) {
		ptxesleft = 1;
		_shop_ptx_mark_as_seen(ssp, ixstr);
	    }
	}
	if (!ptxesleft) {
	    continue;
	}

	/*
	 * Evaluate individual path states. The format allows for
	 * pathstate keys to be enumerated, like S1+S2+S3+S4, or
	 * for ranges in the form "S1-4".
	 */
	for (pskey = util_strsep(&pskeys, FS2);
		pskey && _shop_ptx_count(ssp);
		pskey = util_strsep(&pskeys, FS2)) {
	    CS end;

	    if ((end = strchr(pskey, '-'))) {
		uint64_t i, first, last;
		char nkey[64], *p;

		// The following relies on the fact that Integer.toString
		// is guaranteed to use lower-case characters.
		*end++ = '\0';
		for (p = nkey; ISALPHA(*pskey) && ISUPPER(*pskey); ) {
		    *p++ = *pskey++;
		}
		first = strtol(pskey, NULL, RMAP_RADIX);
		last  = strtol(end, NULL, RMAP_RADIX);
		for (i = first; i <= last; i++) {
		    util_format_to_radix(RMAP_RADIX, p, charlen(nkey), i);
		    _shop_cmp_pathstate(ssp, nkey, ptxes1);
		}
	    } else {
		_shop_cmp_pathstate(ssp, pskey, ptxes1);
	    }
	}
    }

    ps_destroy(CurrentPS);
    CurrentPS = NULL;
}

static shop_e
_shop_collect_targets(shopping_state_s *ssp, CCS cmdix)
{
    char key[64];
    struct cdb_find cdbf_tgt;
    unsigned vlen;
    shop_e rc;

    rc = SHOP_RECYCLED;

    vb_printf(VB_SHOP, "COLLECTING: [%s]", cmdix);

    // First we collect all targets of the winning command in the
    // "real" CA object ....

    snprintf(key, charlen(key), ">%s", cmdix);
    if (cdb_findinit(&cdbf_tgt, ssp->cdbp, key, strlen(key)) < 0) {
	putil_die("cdb_findinit: %s (%s)", key, ca_get_line(ssp->ca));
    }

    while (cdb_findnext(&cdbf_tgt) > 0) {
	CS buf, pskeys, csv, ptxes, ixstr;

	CCS pskey;

	vlen = cdb_datalen(ssp->cdbp);
	buf = (CS)alloca(vlen + 1);
	cdb_read(ssp->cdbp, buf, vlen, cdb_datapos(ssp->cdbp));
	buf[vlen] = '\0';

	if (!(ptxes = strstr(buf, FS1))) {
	    putil_int("bad format in roadmap: %s", buf);
	    rc = SHOP_ERR;
	    continue;
	}

	*ptxes++ = '\0';
	pskeys = buf;

	// Make sure to skip all targets not associated with the winning PTX.
	for (ixstr = util_strsep(&ptxes, FS2);
	     ixstr; ixstr = util_strsep(&ptxes, FS2)) {
	    if (!strcmp(ixstr, ssp->wix)) {
		ps_o tgt_ps;

		pa_o dummy_pa;

		for (pskey = util_strsep(&pskeys, FS2);
		     pskey && rc == SHOP_RECYCLED;
		     pskey = util_strsep(&pskeys, FS2)) {

		    if (cdb_find(ssp->cdbp, pskey, strlen(pskey)) <= 0) {
			putil_int("bad key in roadmap: %s", pskey);
			continue;
		    }

		    vlen = cdb_datalen(ssp->cdbp);
		    csv = (CS)alloca(vlen + 1);
		    cdb_read(ssp->cdbp, csv, vlen, cdb_datapos(ssp->cdbp));
		    csv[vlen] = '\0';
		    vb_printf(VB_SHOP, "COLLECTED [%s] %s", pskey, csv);
		    tgt_ps = ps_newFromCSVString(csv);

		    // Unfortunately, as noted elsewhere, a CA object
		    // doesn't hold PS objects, it holds PA objects.
		    // So we need a dummy PA to wrap the target PS in.
		    dummy_pa = pa_new();
		    pa_set_ps(dummy_pa, tgt_ps);
		    if (ps_is_link(tgt_ps)) {
			pa_set_op(dummy_pa, OP_LINK);
		    } else if (ps_is_symlink(tgt_ps)) {
			pa_set_op(dummy_pa, OP_SYMLINK);
		    } else if (ps_is_unlink(tgt_ps)) {
			pa_set_op(dummy_pa, OP_UNLINK);
		    } else {
			pa_set_op(dummy_pa, OP_CREAT);
		    }
		    pa_set_call(dummy_pa, "dummy");
		    pa_set_uploadable(dummy_pa, 1);
		    ca_record_pa(ssp->ca, dummy_pa);
		}
		// Once we've reached the winner, we're done.
		break;
	    }
	}
    }

    return rc;
}

static int
_shop_process_targets(shopping_state_s *ssp)
{
    int rc;

    // Note: coalescence here will tend to rely on *file* times
    // rather than *op* times since we're relying on dummy PAs.
    // Thus in the case of commands repeatedly writing to the
    // same file we want to end up with the last version.
    ca_coalesce(ssp->ca);

    rc = ca_foreach_cooked_pa(ssp->ca, _shop_process_target, ssp);

    return rc;
}

static shop_e
_shop_for_cmd(shopping_state_s *ssp, CCS cmdix)
{
    CCS line;
    unsigned len;
    CS cmdstate;
    CS pccode, has_tgt, agg, duration, kids, pathcode, rwd;
    int aggregated;

    line = ca_get_line(ssp->ca);

    if (cdb_find(ssp->cdbp, cmdix, strlen(cmdix)) <= 0) {
	putil_int("missing cmd at index=%s", cmdix);
	return SHOP_ERR;
    }

    len = cdb_datalen(ssp->cdbp);
    cmdstate = (CS)alloca(len + 1);
    cdb_read(ssp->cdbp, cmdstate, len, cdb_datapos(ssp->cdbp));
    cmdstate[len] = '\0';

    // This is one place where the number and order of fields is hard wired.
    // *INDENT-OFF*
    if (!(pccode = util_strsep(&cmdstate, FS1)) ||
	    !(pathcode = util_strsep(&cmdstate, FS1)) ||
	    !(has_tgt = util_strsep(&cmdstate, FS1)) ||
	    !(agg = util_strsep(&cmdstate, FS1)) ||
	    !(kids = util_strsep(&cmdstate, FS1)) ||
	    !(duration = util_strsep(&cmdstate, FS1)) ||
	    !(rwd = util_strsep(&cmdstate, FS1))) {
	cdb_read(ssp->cdbp, cmdstate, len, cdb_datapos(ssp->cdbp));
	cmdstate[len] = '\0';
	putil_int("bad format: '%s'", cmdstate);
	return SHOP_ERR;
    }
    // *INDENT-ON*

    aggregated = IS_TRUE(agg);

    vb_printf(VB_SHOP, "%sCMD MATCH: [%s] (%s) %s",
	      aggregated ? "AGGREGATED " : "", cmdix, rwd ? rwd : "", line);

    // If a command has no targets it is ineligible for
    // shopping and must run. Typically this would be
    // something like "echo blah blah".
    if (!IS_TRUE(has_tgt)) {
	CCS msg = "has no targets";

	vb_printf(VB_SHOP, "COMMAND invalidated due to '%s': [%s] %s",
	    msg, cmdix, line);

	return aggregated ? SHOP_MUSTRUN_AGG : SHOP_MUSTRUN;
    }

    // If a command has children we make it ineligible.
    // The problem is a command which both creates files
    // *and* runs children - if we recycle it then the
    // children are not run, even if they would have created
    // other (potentially recyclable) files. There may be an
    // answer in recursive shopping but it would be very complex,
    // so currently we take the easy way out and shop only at
    // the leaves of the tree.
    if (!CSV_FIELD_IS_NULL(kids)) {
	CCS msg = "has children";

	if (vb_bitmatch(VB_SHOP)) {
	    vb_printf(VB_SHOP, "COMMAND invalidated due to '%s': [%s] %s",
		msg, cmdix, line);
	} else {
	    vb_printf(VB_WHY, "COMMAND invalidated due to '%s'", msg);
	}

	return SHOP_MUSTRUN;
    }

    // Removed pccode chck here - could't see its value.

    // There is no current use for this ...
    //ca_set_duration_str(ssp->ca, strtoul(duration, NULL, 10));

    // No current use for pathcode ...

    // Make sure there are still PTXes left before we go shopping.
    if (!_shop_ptx_count(ssp)) {
	return SHOP_NOMATCH;
    }

    // We have now matched a command and parsed its metadata, and
    // still have PTXes in which it was run.
    // It's time to compare it against the current path states.
    // Look first at member prereqs, then at the general public.
    // This is because members are far more likely to change, so
    // shopping can "fail fast" if it considers them first. The
    // main thing here is to limit the number of stats and especially
    // dcode (checksumming) events because they are expensive.
    _shop_compare_prereqs(ssp, cmdix);

    if (_shop_ptx_count(ssp) && _shop_ptx_winner(ssp)) {
	snprintf(ssp->wincmd, charlen(ssp->wincmd), "%s", cmdix);
	return SHOP_RECYCLED;
    } else {
	if (aggregated) {
	    return SHOP_NOMATCH_AGG;
	} else {
	    return SHOP_NOMATCH;
	}
    }
}

/// Initializes shopping data structures.
void
shop_init(void)
{
    CCS rmap;
    int fd;

    // Convenience.
    if (vb_bitmatch(VB_SHOP)) {
	vb_addbit(VB_WHY);
    }

    // Open the roadmap file, and initialize the CDB infrastructure.
    if ((rmap = prop_get_str(P_ROADMAPFILE))) {
	fd = open64(rmap, O_RDONLY, 0);
	if (fd == -1) {
	    putil_syserr(0, rmap);
	} else {
	    struct stat64 stbuf;

	    // Optimization: if the roadmap is zero-length, remove it
	    // now and turn off shopping completely.
	    if (fstat64(fd, &stbuf)) {
		putil_syserr(2, rmap);
	    }
	    if (stbuf.st_size > 0) {
		ShopCDB = (struct cdb *)putil_malloc(sizeof(*ShopCDB));
		cdb_init(ShopCDB, fd);
		close(fd);
	    } else {
		close(fd);
		vb_printf(VB_SHOP, "NO ROADMAP, NO SHOPPING", rmap);
		shop_fini();
	    }
	}
    }
}

/// Finalizes shopping data structures.
void
shop_fini(void)
{
    CCS rmap;

    // Remove the roadmap file.
    if ((rmap = prop_get_str(P_ROADMAPFILE))) {
	if (!prop_is_true(P_LEAVE_ROADMAP)) {
	    if (unlink(rmap)) {
		putil_syserr(0, rmap);
	    } else {
		vb_printf(VB_SHOP, "REMOVED ROADMAP FILE %s", rmap);
	    }
	}
	prop_unset(P_ROADMAPFILE, 0);
    }

    if (ShopCDB) {
	cdb_free(ShopCDB);
	ShopCDB = NULL;
    }
}

/// Returns the number of successfully recycled files (downloaded
/// or reused).
/// @return the number of recycled files
int
shop_get_count(void)
{
    return RecycledCount;
}

/// Traverses the roadmap file, attempting to find build-avoidance
/// opportunities for the current command. Any files eligible for
/// recycling are compared to the current state and downloaded
/// as required.
/// @param[in] ca               the CA being shopped for
/// @param[in] cmdkey           the roadmap index to the command
/// @param[in] getfiles         boolean - really get new files?
/// @return an enum indicating what happened
shop_e
shop(ca_o ca, CCS cmdkey, int getfiles)
{
    shop_e rc;
    shopping_state_s shop_state, *ssp = &shop_state;
    struct cdb_find cdbf;
    CCS line;
    char cmdix[64];

    if (!ShopCDB) {
	return SHOP_OFF;
    }

    // Expect the worst, hope for the best ...
    rc = SHOP_NOMATCH;

    memset(ssp, 0, sizeof(*ssp));
    ssp->getfiles = getfiles;
    ssp->cdbp = ShopCDB;
    ssp->ca = ca;
    ssp->ptx_dict = _shop_ptx_init();
    ssp->ignore_path_re = re_init_prop__(P_SHOP_IGNORE_PATH_RE);

    // First, grab the set of PTXes and store them away.
    if (cdb_findinit(&cdbf, ssp->cdbp, PTX_PREFIX, strlen(PTX_PREFIX)) >= 0) {
	while (cdb_findnext(&cdbf) > 0) {
	    unsigned len;

	    char xn[64];

	    CS id;

	    len = cdb_datalen(ssp->cdbp);
	    cdb_read(ssp->cdbp, xn, len, cdb_datapos(ssp->cdbp));
	    xn[len] = '\0';
	    if ((id = strchr(xn, '='))) {
		*id++ = '\0';
		_shop_ptx_insert(ssp, xn, id);
	    } else {
		putil_int("bad PTX line in roadmap: %s", xn);
	    }
	}
    } else {
	putil_die("cdb_findinit (%s)", PTX_PREFIX);
    }

    // This is basically a debugging mode where the cmd key is
    // provided directly. We then look up the cmdline from that,
    // and rejoin the main logic.
    if (cmdkey) {
	if ((line = _shop_find_cmdline(ssp, cmdkey))) {
	    ca_set_line(ssp->ca, line);
	    putil_free(line);
	} else {
	    putil_int("no line found for cmd key '%s'", cmdkey);
	    rc = SHOP_ERR;
	}
    }

    line = ca_get_line(ssp->ca);

    // Tell CDB what command line we're looking for.
    if (cdb_findinit(&cdbf, ssp->cdbp, line, strlen(line)) < 0) {
	putil_die("cdb_findinit (%s)", line);
    }

    // It's possible there would be more than one instance of a given
    // command line so we look for the full match in a loop. In most
    // cases this loop will be traversed only once.
    for (cmdix[0] = '\0'; rc != SHOP_RECYCLED && cdb_findnext(&cdbf) > 0;) {
	unsigned len;

	len = cdb_datalen(ssp->cdbp);
	cdb_read(ssp->cdbp, cmdix, len, cdb_datapos(ssp->cdbp));
	cmdix[len] = '\0';

	rc = _shop_for_cmd(ssp, cmdix);
    }

    // If rc equals SHOP_RECYCLED we've matched a command from a given PTX.
    // All we need to do is determine all the target files which
    // go with it and process them.
    if (rc == SHOP_RECYCLED) {
	vb_printf(VB_SHOP, "WINNER is %s (%s)", ssp->winner, ssp->wix);
	if (_shop_collect_targets(ssp, ssp->wincmd) == SHOP_RECYCLED) {
	    if (_shop_process_targets(ssp)) {
		rc = SHOP_ERR;
	    } else {
		// Mark the passed-in CA as recycled.
		ca_set_recycled(ssp->ca, ssp->winner);
	    }
	}
    }

    // All the rest is cleanup ...

    ca_clear_pa(ssp->ca);

    _shop_ptx_destroy(ssp->ptx_dict);

    return rc;
}
