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
/// @brief Support for standard timestamp format.
/// Some interfaces report time in seconds, others in milli-, micro-,
/// or nano-seconds. Sometimes these differ between platforms (Linux
/// stat() reports file times to microseconds, Solaris to nanoseconds,
/// and Windows is even more different, thinking in 100-nanosecond
/// intervals sometimes referred to as "clunks"). We aim for the
/// "highest common denominator" by using the moment_s type to store
/// time in terms of seconds plus nanoseconds. All other time values
/// are multiplied appropriately to fit this format. Thus, note
/// that though we store and report 'nanoseconds', almost no platform
/// actually provides that resolution. Even Solaris appears to simply
/// report (microseconds * 1000) and call them nanoseconds.

#include "AO.h"

#include <math.h>

#include "MOMENT.h"
#include "PROP.h"

/// @cond static
#define MILLIS_PER_SECOND				1000
#define NANOS_PER_MILLI					1000000		
#define NANOS_PER_SECOND				1000000000
/// @endcond static

/// Returns true if the provided time value is set to a legal time
/// value later than the epoch.
/// @param[in] moment   a moment_s struct to be checked
/// @return 0 if the time value is at or before the epoch
int
moment_is_set(moment_s moment)
{
    return (moment.ntv_sec + (moment.ntv_nsec / NANOS_PER_SECOND)) > 0;
}

/// Returns the current system time as a 'moment'.
/// Similar to gettimeofday() but (a) works on Windows and Unix and
/// (b) uses nanosecond units rather than microseconds. Note that there's
/// no magic; actual time resolution is that of the underlying platform
/// (which is microseconds on Unix and 10 milliseconds on Windows).
/// @param[out] mptr    a pointer to a moment_s to be filled in
/// @return 0 on success
int
moment_get_systime(moment_s *mptr)
{
#if defined(_WIN32)
    // This implementation is taken from 'c-ares' at
    // http://daniel.haxx.se/projects/c-ares/.
    FILETIME ft;
    LARGE_INTEGER li;
    __int64 clunks;

    // At least somebody refers to the 100-nanosecond intervals
    // that Windows measures as 'clunks'.
    GetSystemTimeAsFileTime(&ft);
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    clunks = li.QuadPart;	// In 100-nanosecond intervals
    clunks -= WIN_EPOCH_OFFSET;	// Convert to Unix epoch
    mptr->ntv_sec = (int64_t)(clunks / WIN_CLUNKS_PER_SEC);
    mptr->ntv_nsec = (long)((clunks % WIN_CLUNKS_PER_SEC) * 100);
#else				/*!_WIN32 */
    struct timeval tv;

    if ((gettimeofday(&tv, NULL) == -1)) {
	putil_syserr(2, _T("gettimeofday()"));
    }
    mptr->ntv_sec = tv.tv_sec;
    mptr->ntv_nsec = tv.tv_usec * 1000;
#endif	/*_WIN32*/

    return 0;
}

/// Sets the modification time of the specified file to the specified time.
/// A time parameter of NULL means 'right now'.
/// @param[in] mptr     pointer to the new moment, or NULL
/// @param[in] path     string representing the pathname to be changed
/// @return 0 on success
int
moment_set_mtime(moment_s *mptr, CCS path)
{
#if defined(_WIN32)
    HANDLE hFile;
    FILETIME ft;

    hFile = CreateFile(path, GENERIC_READ | FILE_WRITE_ATTRIBUTES,
		       FILE_SHARE_READ, NULL, OPEN_EXISTING,
		       FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
	putil_win32err(0, GetLastError(), path);
	return -1;
    }

    if (mptr) {
	__int64 clunks;

	clunks = (mptr->ntv_sec * WIN_CLUNKS_PER_SEC) + WIN_EPOCH_OFFSET;
	clunks += mptr->ntv_nsec / 100;	// add in the nanosecond part
	ft.dwLowDateTime = (DWORD)clunks;
	ft.dwHighDateTime = (unsigned long)(clunks >> 32);
    } else {
	SYSTEMTIME st;

	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
    }

    if (SetFileTime(hFile, NULL, NULL, &ft) == 0) {
	putil_win32err(0, GetLastError(), path);
	return -1;
    }

    if (!CloseHandle(hFile)) {
	putil_win32err(0, GetLastError(), path);
    }
#else	/*_WIN32*/
    if (mptr) {
	struct timeval tvp[2];

	// Might as well set access time same as mod time
	// NOTE: there is a loss of precision (64 bits => 32) here
	// until the day when a time_t is 64 bits.
	tvp[0].tv_sec = tvp[1].tv_sec = (time_t)mptr->ntv_sec;

	// We store nanoseconds but can only set to the granularity
	// of microseconds ...
	tvp[0].tv_usec = tvp[1].tv_usec = mptr->ntv_nsec / 1000;

	if (utimes(path, tvp)) {
	    putil_syserr(0, path);
	    return -1;
	}
    } else {
	if (utimes(path, NULL)) {
	    putil_syserr(0, path);
	    return -1;
	}
    }
#endif	/*_WIN32*/

    return 0;
}

/// Subtracts the second time value from the first.
/// The result is stored in the pointer if not null.
/// @param[in] left     a moment_s representing the first time value
/// @param[in] right    a moment_s representing the second time value
/// @param[out] delta   a pointer to a moment_s for the delta, or NULL
/// @return 1, 0, or -1 as left is greater than, equal to, or less than right
int
moment_cmp(moment_s left, moment_s right, moment_s *delta)
{
    static long roundoff = -1;
    uint64_t l, r;
    int64_t d;

    // Initialize this on the first pass through.
    if (roundoff == -1) {
	roundoff = prop_get_long(P_SHOP_TIME_PRECISION);
	roundoff = (long)pow((double)10, (9 - roundoff));
    }

    l = left.ntv_sec * NANOS_PER_SECOND + left.ntv_nsec;
    r = right.ntv_sec * NANOS_PER_SECOND + right.ntv_nsec;
    d = l - r;

    // Calculate the optional delta *before* any roundoff occurs.
    if (delta) {
	delta->ntv_sec  = d / NANOS_PER_SECOND;
	delta->ntv_nsec = (long)(d % NANOS_PER_SECOND);
    }

    // Optionally round off before comparing.
    if (roundoff != -1) {
	l = (l / roundoff) * roundoff;
	r = (r / roundoff) * roundoff;
	d = l - r;
    }

    if (d == 0) {
	return 0;
    } else if (d > 1) {
	return 1;
    } else {
	return -1;
    }
}

/// Subtracts a starting Moment from an ending Moment to derive the
/// duration of an event.
/// @param[in] ended    a moment_s representing the ending time
/// @param[in] started  a moment_s representing the starting time
/// @return the duration in milliseconds
unsigned long
moment_duration(moment_s ended, moment_s started)
{
    moment_s delta;
    unsigned long duration;

    (void)moment_cmp(ended, started, &delta);

    duration = ((unsigned long)delta.ntv_sec * MILLIS_PER_SECOND) +
	(delta.ntv_nsec / NANOS_PER_MILLI);

    return duration;
}

/// Parses a textual moment_s field back into a binary form.
/// @param[out] mptr    a pointer to the moment_s to be filled in
/// @param[in] str      a stringified form of the moment_s format
/// @return 0 on success
int
moment_parse(moment_s *mptr, CCS str)
{
    TCHAR mbuf[MOMENT_BUFMAX], *nsec;

    _tcscpy(mbuf, str);
    if ((nsec = _tcschr(mbuf, '.'))) {
	*nsec++ = '\0';
	mptr->ntv_sec = putil_strtoll(mbuf, NULL, CSV_RADIX);
	mptr->ntv_nsec = _tcstol(nsec, NULL, CSV_RADIX);
    } else {
	memset(mptr, 0, sizeof(moment_s));
	return 1;
    }

    return 0;
}

/// Formats a Moment into the canonical string format. This may use
/// a radix higher than 10, such as 36, for a more compact string.
/// @param[in] moment   the moment struct to be formatted
/// @param[out] buf     a buffer to hold the formatted string
/// @param[in] bufmax   the size of the passed-in buffer
/// @return the formatted string
CCS
moment_format(moment_s moment, CS buf, size_t bufmax)
{
    TCHAR secbuf[MOMENT_BUFMAX], nsecbuf[MOMENT_BUFMAX];

    buf[0] = '\0';
    (void)util_format_to_radix(CSV_RADIX, secbuf, charlen(secbuf),
	moment.ntv_sec);
    (void)util_format_to_radix(CSV_RADIX, nsecbuf, charlen(nsecbuf),
	moment.ntv_nsec);
    _sntprintf(buf, bufmax, "%s.%s", secbuf, nsecbuf);
    return buf;
}

/// Formats a Moment into a human-readable ISO8601 string format.
/// @param[in] moment   the moment struct to be formatted
/// @param[out] buf     a buffer to hold the formatted string
/// @param[in] bufmax   the size of the passed-in buffer
/// @return the formatted string
CCS
moment_format_vb(moment_s moment, CS buf, size_t bufmax)
{
    time_t seconds;
    struct tm *ptm;
    TCHAR lbuf[256];

    seconds = (time_t)moment.ntv_sec;

    if (!(ptm = localtime(&seconds))) {
	putil_syserr(2, _T("localtime()"));
    }

    if (!strftime(lbuf, charlen(lbuf), "%H:%M:%S", ptm)) {
	putil_syserr(2, _T("strftime()"));
    }

    _sntprintf(buf, bufmax, "%s,%03ld",
	lbuf, moment.ntv_nsec / NANOS_PER_MILLI);
    return buf;
}

/// Formats a Moment into a standard id-style string
/// @param[in] moment   pointer to a struct to be formatted
/// @param[out] buf     a buffer to hold the formatted string
/// @param[in] bufmax   the size of the passed-in buffer
/// @return the formatted string
CCS
moment_format_id(moment_s *mp, CS buf, size_t bufmax)
{
    time_t seconds;
    struct tm *ptm;

    if (mp) {
	seconds = (time_t)mp->ntv_sec;
    } else {
	moment_s m2;
	moment_get_systime(&m2);
	seconds = (time_t)m2.ntv_sec;
    }

    if (!(ptm = gmtime(&seconds))) {
	putil_syserr(2, _T("gmtime()"));
    }

    if (!strftime(buf, bufmax, "%Y%m%d%H%M%S", ptm)) {
	putil_syserr(2, _T("strftime()"));
    }

    return buf;
}
