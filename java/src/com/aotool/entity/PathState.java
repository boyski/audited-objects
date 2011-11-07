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
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;

import javax.persistence.Basic;
import javax.persistence.Embeddable;
import javax.persistence.Embedded;

import com.aotool.Constants;
import com.aotool.Messages;

/**
 * PathState is an entity class which represents a particular state (contents)
 * of a given PathName. A PathState recorded on the server may no longer exist
 * on any client platform, but it must have existed at one point on one client
 * and been sampled at that time in order to end up here.
 * <p>
 * </p>
 * A PathState refers to a PathName and stores only content data. Two files with
 * the same PathName and identical contents are considered the same PathState
 * even if the owner, mode, timestamp, or other metadata differ.
 */
@Embeddable
public class PathState implements Comparable<Object>, Serializable {

    private static final long serialVersionUID = 1L;

    /** The number of fields the CSV line is split into. */
    private static final int NUMBER_OF_PATHSTATE_FIELDS = 8;

    /** The state. */
    private String psbuf;

    /** The path name. */
    @Embedded
    private PathName pathName;

    /** The state name. */
    @Basic(optional = true)
    private String stateName;

    private transient String[] fields;

    /**
     * Instantiates a PathState object. Required by JPA for entity classes.
     */
    protected PathState() {
        super();
    }

    /**
     * Instantiates a new PathState object/
     * 
     * @param pathName
     *            the path name
     * @param psbuf
     *            the psbuf
     */
    private PathState(PathName pathName, String psbuf) {
        this.pathName = pathName;
        this.psbuf = psbuf;
    }

    /**
     * Gets the data type.
     * 
     * @return the data type
     */
    public String getDataType() {
        return getField(0);
    }

    /**
     * Gets the name of the filesystem in which this path exists.
     * 
     * @return the filesystem name
     */
    public String getFsName() {
        return getField(1);
    }

    /**
     * Gets the file time stamp.
     * 
     * @return the time stamp
     */
    public Moment getTimeStamp() {
        return new Moment(getField(2));
    }

    /**
     * Gets the file size.
     * 
     * @return the size
     */
    public long getSize() {
        return Long.parseLong(getField(3));
    }

    /**
     * Gets the file mode (a Unix concept which is simulated elsewhere).
     * 
     * @return the mode
     */
    public int getMode() {
        return Integer.parseInt(getField(4), Constants.CSV_RADIX);
    }

    public String getModeString() {
        return getField(4);
    }

    /**
     * Gets the data code.
     * 
     * @return the data code
     */
    public String getDataCode() {
        return getField(5);
    }

    /**
     * Checks for the existence of a data code.
     * 
     * @return true, if a data code has been calculated
     */
    public boolean hasDataCode() {
        String code = getDataCode();
        return code != null && code.length() != 0;
    }

    /**
     * Gets the link target.
     * 
     * @return the link target
     */
    public String getLinkTarget() {
        String decoded = null;
        try {
            // The link target is supplied URL-encoded just in case it
            // contains an embedded comma which would break the CSV format.
            decoded = URLDecoder.decode(getField(6), "UTF-8"); //$NON-NLS-1$
        } catch (final UnsupportedEncodingException e) {
            // We feel confident that UTF-8 is a supported encoding.
            e.printStackTrace();
        }
        return decoded;
    }

    /**
     * Gets the state name.
     * 
     * @return the state name
     */
    public String getStateName() {
        return this.stateName;
    }

    /**
     * Sets the state name.
     * 
     * @param stateName
     *            the new state name
     */
    public void setStateName(String stateName) {
        this.stateName = stateName;
    }

    /**
     * Gets the PathName.
     * 
     * @return the path name
     */
    public PathName getPathName() {
        return this.pathName;
    }

    /**
     * Returns the pathname in the internal canonical form, which is "/"
     * separated in the manner of Unix paths.
     * 
     * @return the pathname in canonical form
     */
    public String getPathString() {
        return this.getPathName().getPathString();
    }

    private String getField(int index) {
        String field = null;
        if (fields == null) {
            fields = this.psbuf.split(Constants.FS1, NUMBER_OF_PATHSTATE_FIELDS);
        }
        if (index < fields.length && !Constants.CSV_NULL_FIELD.equals(fields[index])) {
            field = fields[index];
        }
        return field;
    }

    public boolean isFile() {
        return "f".equals(getDataType());
    }

    /**
     * Returns a comma-separated-values (CSV) string which represents the
     * current state of the object. This can be thought of as a serialization
     * format, though it is not a true serialization because only "important"
     * state is preserved.
     * 
     * @return a comma-separated string representing the object state
     */
    public String toCSVString() {
        return psbuf;
    }

    public String toStandardString() {
        StringBuilder sb = new StringBuilder();
        sb.append(psbuf);
        sb.append(Constants.FS1);
        sb.append(pathName.getPathString());
        if (this.stateName != null) {
            sb.append(" [");
            sb.append(this.stateName);
            sb.append(']');
        }
        return sb.toString();
    }

    @Override
    public String toString() {
        return toStandardString();
    }

    /**
     * Compare file modification times between this PathState and another.
     * 
     * @param other
     *            the PathState object to be compared against this one
     * 
     * @return a negative integer, zero, or a positive integer as this object is
     *         less than, equal to, or greater than the specified object.
     */
    public int compareMtime(PathState other) {
        final Moment thisTime = this.getTimeStamp();
        final Moment thatTime = other.getTimeStamp();
        return thisTime.compareTo(thatTime);
    }

    public int compareTo(Object o) {

        final PathState other = (PathState) o;
        int result;

        // Between two states of the same path, the newer should
        // compare greater such that listings sort oldest to newest.
        if (this == other) {
            return 0;
        } else {
            if ((result = this.getPathString().compareTo(other.getPathString())) != 0) {
                return result;
            } else {
                final int timecmp = this.compareMtime(other);
                if (timecmp != 0) {
                    if (this.getSize() != other.getSize()) {
                        return timecmp;
                    } else if (this.hasDataCode()) {
                        if (this.getDataCode().compareTo(other.getDataCode()) != 0) {
                            return timecmp;
                        } else {
                            return 0;
                        }
                    } else {
                        return timecmp;
                    }
                } else {
                    if (this.getSize() != other.getSize()) {
                        return this.getSize() - other.getSize() > 0 ? 1 : -1;
                    } else if (this.hasDataCode()
                            && (result = this.getDataCode().compareTo(other.getDataCode())) != 0) {
                        return result;
                    } else {
                        return 0;
                    }
                }
            }
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
        // Remember to stay "consistent with equals" here.
        int result = 17;
        result = (result * 37) + this.getPathString().hashCode();
        result = (result * 37) + new Long(this.getSize()).hashCode();
        result = (result * 37) + this.getDataCode().hashCode();
        return result;
    }

    /**
     * A Builder class for PathState. Converts a CSV line into a PathState
     * object.
     */
    public static class Builder {

        /** The CSV string. */
        private final String csv;

        /**
         * Instantiates a new builder.
         * 
         * @param csv
         *            the csv
         */
        public Builder(String csv) {
            this.csv = csv;
        }

        /**
         * Builds the PathState object.
         * 
         * @return the path state
         */
        public PathState build() {
            final String psbuf = csv.substring(0, csv.lastIndexOf(Constants.FS1));
            final String pnbuf = csv.substring(csv.lastIndexOf(Constants.FS1) + 1);
            if (pnbuf.length() == 0) {
                throw new IllegalArgumentException(Messages.getString("PathState.0") + ' ' + csv); //$NON-NLS-1$
            }
            final PathName pn = new PathName.Builder(pnbuf).build();
            final PathState ps = new PathState(pn, psbuf);
            return ps;
        }
    }
}
