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
/// @brief Support for standard timestamp format.
/// MODERN:
/// Nanosecond resolution is assumed.
/// HISTORICAL:
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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <math.h>

#include "MOMENT.h"
#include "PROP.h"
#include "TRY.h"

/// @cond static
#define NANOS_PER_SECOND                                1000000000u
/// @endcond static

/// True iff the provided time value is legal and later than the epoch.
/// @param[in] moment   a moment_s struct to be checked
/// @return false if the time value is at or before the epoch
bool
moment_is_set(moment_s moment)
{
    return (moment.ntv_sec + (moment.ntv_nsec % NANOS_PER_SECOND)) > 0;
}

/// Return the current system time as a 'moment' with nanosecond precision.
/// @param[out] mptr    a pointer to a moment_s to be filled in
void
moment_get_systime(moment_s *mptr)
{
    struct timespec ts;

    TRY(clock_gettime(CLOCK_REALTIME, &ts) != -1, "clock_gettime()");
    mptr->ntv_sec = ts.tv_sec;
    mptr->ntv_nsec = ts.tv_nsec;
}

/// Set the modification time of the specified file to the specified time.
/// A time parameter of NULL means 'right now'.
/// @param[in] mptr     pointer to the new moment, or NULL
/// @param[in] path     string representing the pathname to be changed
void
moment_set_mtime(moment_s *mptr, CCS path)
{
    struct timespec times[2];
    // Might as well set access time same as mod time.
    if (mptr) {
        times[0].tv_sec = times[1].tv_sec = mptr->ntv_sec;
        times[0].tv_nsec = times[1].tv_nsec = mptr->ntv_nsec;
    } else {
        times[0].tv_sec = times[1].tv_sec = UTIME_NOW;
        times[0].tv_nsec = times[1].tv_nsec = UTIME_NOW;
    }
    TRY(utimensat(AT_FDCWD, path, times, 0) != -1, "utimensat()");
}

/// Subtract the second time value from the first.
/// The result is stored in the pointer if not null.
/// @param[in] left     moment_s representing the first time value
/// @param[in] right    moment_s representing the second time value
/// @param[out] delta   pointer to a moment_s for the delta, or NULL
/// @return 1, 0, or -1 as left is greater than, equal to, or less than right
int
moment_cmp(moment_s left, moment_s right, moment_s *delta)
{
    unsigned long long l, r;
    long long d;

    l = left.ntv_sec * NANOS_PER_SECOND + left.ntv_nsec;
    r = right.ntv_sec * NANOS_PER_SECOND + right.ntv_nsec;
    d = l - r;

    // Calculate the optional delta.
    if (delta) {
        delta->ntv_sec  = d / NANOS_PER_SECOND;
        delta->ntv_nsec = (long)(d % NANOS_PER_SECOND);
    }

    if (d == 0) {
        return 0;
    } else if (d > 1) {
        return 1;
    } else {
        return -1;
    }
}

/// Fill in a new Moment describing the time since the supplied Moment.
/// @param[in] delta  pointer to a moment_s which receives the delta
/// @param[in] since  a moment_s representing the starting time
/// @return the duration in milliseconds
int
moment_since(moment_s since, moment_s *delta)
{
    moment_s now;

    moment_get_systime(&now);
    return moment_cmp(now, since, delta);
}

/// Subtract a starting Moment from an ending Moment to derive the
/// duration of an event. Note that while times are measured from
/// the epoch and require a long long, it's assumed that durations
/// will fit in a long since they're relative to a much more
/// recent time.
/// @param[in] ended    a moment_s representing the ending time
/// @param[in] started  a moment_s representing the starting time
/// @return the duration in nanoseconds
unsigned long
moment_duration(moment_s ended, moment_s started)
{
    moment_s delta;

    (void)moment_cmp(ended, started, &delta);
    return (delta.ntv_sec * NANOS_PER_SECOND) + delta.ntv_nsec;
}

/// Format a Moment into the canonical string format. May use
/// a radix higher than 10, such as 36, for a more compact string.
/// @param[in] moment   the moment struct to be formatted
/// @param[out] buf     a buffer to hold the formatted string
/// @param[in] bufmax   the size of the passed-in buffer
/// @return the formatted string
CCS
moment_format(moment_s moment)
{
    char buf[64]; // easily big enough to hold any formatted moment
    (void)util_format_to_radix(COMPACT_RADIX, buf, sizeof(buf),
        (moment.ntv_sec * NANOS_PER_SECOND) + moment.ntv_nsec);
    return strdup(buf);
}

/// Parse a textual moment_s field back into a binary form.
/// @param[out] mptr    pointer to the moment_s to be filled in
/// @param[in] str      stringified form of the moment_s format
/// @return 0 on success
int
moment_parse(moment_s *mptr, CCS str)
{
    unsigned long long nanos = strtoull(str, NULL, COMPACT_RADIX);
    mptr->ntv_sec = nanos / NANOS_PER_SECOND;
    mptr->ntv_nsec = nanos % NANOS_PER_SECOND;
    assert(((mptr->ntv_sec * NANOS_PER_SECOND) + mptr->ntv_nsec) == nanos);
    return 0;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
