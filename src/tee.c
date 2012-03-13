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
/// @brief Allows this program to run as a "tee"-alike process.

#include "AO.h"

#include "MOMENT.h"
#include "PROP.h"
#include "TEE.h"

void
tee_into(const char *into)
{
    FILE *teeout;
    ssize_t nread;
    char buf[BUFSIZ];
    moment_s start, delta;
    char tstamp[MOMENT_BUFMAX];
    char prev_char = '\n';
    ssize_t i;
    int timestamp_prefix = 0;

    if (prop_is_true(P_LOG_TIME_STAMP)) {
	timestamp_prefix = 1;
	moment_get_systime(&start);
    }

    teeout = fopen(into, "w");

    if (!teeout) {
	putil_syserr(2, into);
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(teeout, NULL, _IONBF, 0);

    while (1) {
	nread = read(0, buf, sizeof buf);
	if (nread == 0) {
	    break;
	} else if (nread < 0) {
#ifdef EINTR
	    if (errno == EINTR)
		continue;
	    else
#endif
		putil_syserr(2, "read");
	}

	if (timestamp_prefix) {
	    moment_since(start, &delta);
	    (void)moment_format_milli(delta, tstamp, charlen(tstamp));
	}

	for (i = 0; i < nread; i++) {
	    if (timestamp_prefix && prev_char == '\n') {
		fprintf(stdout, "%s: ", tstamp);
		fprintf(teeout, "%s: ", tstamp);
	    }
	    prev_char = buf[i];
	    putc(buf[i], stdout);
	    putc(buf[i], teeout);
	}
    }

    fflush(NULL);
    fclose(stdout);
    fclose(teeout);

    exit(0);
}
