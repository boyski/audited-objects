/*******************************************************************************
 * Copyright 2002-2011 David Boyce. All rights reserved.
 * 
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
 ******************************************************************************/

package com.aotool.entity;

import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;

import javax.persistence.Embeddable;

import com.aotool.Constants;
import com.aotool.Messages;

/**
 * Moment is an entity class which represents a specific instant in time with
 * nanosecond precision. The time is stored as nanoseconds since the Unix epoch
 * (1/1/1970). Given a 63-bit Long value this allows for something like 292
 * years, according to my calculations, thus failing around the year 2262.
 * <p>
 * </p>
 * Moments are not primarily designed to describe events which take place on the
 * server machine. Rather, they record the times of events reported as having
 * taken place on client systems. Not all client systems are capable of
 * measuring or reporting time to nanosecond precision, but all times reported
 * to the server are normalized to the "nanoseconds" format anyway.
 * <p>
 * </p>
 * There's a subtle client-side problem worth noting here. Though some systems
 * (e.g. Solaris) <b>record</b> file modification times to nanosecond precision,
 * there is no POSIX API to <b>set</b> times with better than microsecond
 * precision. Thus there's no portable way for a Unix program to "touch" a file
 * without truncating the nanosecond part. As a result, comparing file times
 * down to the nanosecond is a bit unfair since the client can't always set
 * those times. For this reason, time comparisons can be told to round to
 * microseconds.
 */
@Embeddable
public class Moment implements Serializable, Comparable<Object> {

    private static final long serialVersionUID = 1L;

    /** The number of milliseconds in one second. */
    private final static long MILLIS_PER_SECOND = 1000;

    /** The number of nanoseconds in one millisecond. */
    private final static long NANOS_PER_MILLI = 1000000;

    /** The number of nanoseconds in one millisecond. */
    private final static long NANOS_PER_SECOND = 1000000000;

    /** Names for months for use in toString methods */
    private final static String[] months = {
            Messages.getString("Moment.0"), Messages.getString("Moment.1"), Messages.getString("Moment.2"), Messages.getString("Moment.3"), Messages.getString("Moment.4"), //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$
            Messages.getString("Moment.5"), Messages.getString("Moment.6"), Messages.getString("Moment.7"), Messages.getString("Moment.8"), Messages.getString("Moment.9"), Messages.getString("Moment.10"), Messages.getString("Moment.11") }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$ //$NON-NLS-6$ //$NON-NLS-7$

    private long nanoseconds;

    /**
     * Instantiates a Moment object. Required by JPA for entity classes.
     */
    public Moment() {
        nanoseconds = System.currentTimeMillis() * NANOS_PER_MILLI;
    }

    /**
     * Instantiates a new Moment.
     * 
     * @param millis
     *            the milliseconds
     */
    public Moment(long millis) {
        nanoseconds = millis * NANOS_PER_MILLI;
    }

    /**
     * Instantiates a new Moment.
     * 
     * @param secs
     *            the seconds
     * @param nanos
     *            the nanoseconds
     */
    public Moment(long secs, long nanos) {
        this.nanoseconds = (secs * NANOS_PER_SECOND) + nanos;
    }

    /**
     * Instantiates a new Moment from the standard string format.
     * 
     * @param string
     *            the string
     */
    public Moment(String string) {
        final String[] fields = string.split("[.]", 2); //$NON-NLS-1$
        final long seconds = Long.parseLong(fields[0], Constants.CSV_RADIX);
        final long nanos = Long.parseLong(fields[1], Constants.CSV_RADIX);
        this.nanoseconds = (seconds * NANOS_PER_SECOND) + nanos;
    }

    /**
     * Gets the value of this Moment in terms of milliseconds since the epoch.
     * 
     * @return the milliseconds value
     */
    public long getMilliSeconds() {
        return nanoseconds / NANOS_PER_MILLI;
    }

    /**
     * Gets the seconds part.
     * 
     * @return the seconds part
     */
    public long getSeconds() {
        return nanoseconds / NANOS_PER_SECOND;
    }

    /**
     * Gets the nanoseconds part.
     * 
     * @return the nanoseconds part
     */
    public long getNanoseconds() {
        return nanoseconds % NANOS_PER_SECOND;
    }

    /**
     * Standard comparator.
     * 
     * @param o
     *            the object to compare against
     * @return int
     */
    public int compareTo(Object o) {
        final Moment comparator = (Moment) o;
        if (this.nanoseconds > comparator.nanoseconds) {
            return 1;
        } else if (this.nanoseconds < comparator.nanoseconds) {
            return -1;
        } else {
            return 0;
        }
    }

    @Override
    public final boolean equals(Object obj) {
        if (this == obj) {
            return true;
        } else if (obj == null) {
            return false;
        } else if (this.getClass() != obj.getClass()) {
            return false;
        } else {
            // I like to handle equality and ordering with the same
            // code; that way you can be sure they're consistent.
            return this.compareTo(obj) == 0;
        }
    }

    @Override
    public final int hashCode() {
        return (int) nanoseconds;
    }

    /**
     * Returns a string version of this Moment in the format used by the Unix ls
     * command. In particular, Unix ls uses a different style for dates more
     * than ~6 months in the past.
     * 
     * @return the ls-style string
     */
    public String toLsStyleString() {
        final Calendar cal = new GregorianCalendar();
        final Date date = new Date(this.getSeconds() * MILLIS_PER_SECOND);
        cal.setTime(date);
        final long dt = date.getTime();
        if (dt == -1L) {
            return "************"; //$NON-NLS-1$
        }
        final long now = (new Date()).getTime();

        String fmt;
        if (Math.abs(now - dt) < (183L * 24L * 60L * 60L * MILLIS_PER_SECOND)) {
            fmt = String.format("%s%3s %02d:%02d", months[cal //$NON-NLS-1$
                    .get(Calendar.MONTH)], cal.get(Calendar.DATE), cal.get(Calendar.HOUR_OF_DAY),
                    cal.get(Calendar.MINUTE));
        } else {
            fmt = String.format("%s%3s  %s", months[cal.get(Calendar.MONTH)], //$NON-NLS-1$
                    cal.get(Calendar.DATE), cal.get(Calendar.YEAR));
        }
        return fmt;
    }

    /**
     * Returns a string version of this Moment in an ISO-standard format.
     * 
     * @return the iSO style string
     */
    public String toISOStyleString() {
        final SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ"); //$NON-NLS-1$
        final long millis = this.getMilliSeconds();
        final String iso = sdf.format(new Date(millis));
        return iso;
    }

    /**
     * Returns a string version of this Moment in the format specified by RFC
     * 822 (as used in the email Date header).
     * 
     * @return the RFC822-style string
     */
    public String toRFC822StyleString() {
        final long millis = this.getMilliSeconds();
        return new Date(millis).toString();
    }

    /**
     * Returns the "standard" string format for this class. The standard format
     * is part of the API and more or less stable, whereas the result of the
     * toString() method is subject to change and best used only for debugging.
     * 
     * @return the standard string format
     */
    public String toStandardString() {
        final StringBuilder sb = new StringBuilder();
        sb.append(Long.toString(getSeconds(), Constants.CSV_RADIX));
        sb.append('.');
        sb.append(Long.toString(getNanoseconds(), Constants.CSV_RADIX));
        return sb.toString();
    }

    @Override
    public String toString() {
        return this.toStandardString();
    }
}
