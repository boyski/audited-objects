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
import java.util.Date;
import java.util.SimpleTimeZone;

import javax.persistence.Embeddable;

/**
 * Duration is an entity class which represents the elapsed time between two
 * Moments with millisecond precision.
 */
@Embeddable
public class Duration implements Serializable {

    private static final long serialVersionUID = 1L;

    /** The number of elapsed milliseconds. */
    private long duration;

    /**
     * Instantiates a Duration object. Required by JPA for entity classes.
     */
    protected Duration() {
    }

    /**
     * Instantiates a new Duration object.
     * 
     * @param millis
     *            the number of duration elapsed
     */
    Duration(long millis) {
        duration = millis;
    }

    /**
     * Constructor from string format.
     * 
     * @param string_form
     *            the stringified format of a Duration
     */
    public Duration(String string_form) {
        duration = Long.parseLong(string_form);
    }

    /**
     * Returns an instance of Duration representing the time elapsed between the
     * two Moments.
     * 
     * @param startTime
     *            the starting Moment
     * @param endTime
     *            the ending Moment
     * 
     * @return a new instance of Duration
     */
    public static Duration difference(Moment startTime, Moment endTime) {
        long start = startTime.getMilliSeconds();
        long end = endTime.getMilliSeconds();
        return new Duration(end - start);
    }

    public static Duration elapsed(Moment past) {
        long start = past.getMilliSeconds();
        long end = System.currentTimeMillis();
        return new Duration(end - start);
    }

    /**
     * Returns the "standard" string format for this class. The standard format
     * is part of the API and more or less stable, whereas the result of the
     * toString() method is subject to change and best used only for debugging.
     * 
     * @return the standard string format
     */
    public String toStandardString() {
        final SimpleDateFormat sdf = new SimpleDateFormat("HH:mm:ss"); //$NON-NLS-1$
        sdf.setTimeZone(new SimpleTimeZone(0, "GMT")); //$NON-NLS-1$
        // add 1/2 second for better roundoff behavior
        final String hhmmss = sdf.format(new Date(duration + 500));
        return hhmmss;
    }

    public String toSimpleString() {
        return String.valueOf(duration);
    }

    @Override
    public String toString() {
        return toStandardString();
    }
}
