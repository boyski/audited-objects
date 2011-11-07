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

import javax.persistence.Column;
import javax.persistence.Embeddable;

/**
 * PathName is an entity class representing a client-side file pathname. Member
 * paths are stored in a project-relative form, while non-members have absolute
 * paths. A PathName need not represent a file which actually exists on any
 * given host; it's merely a legal pathname which may point to valid file data
 * at some times and on some systems.
 */
@Embeddable
public class PathName implements Comparable<Object>, Serializable {

    private static final long serialVersionUID = 1L;

    /** The pathname. */
    @Column(length = 1024)
    private String pName;

    /**
     * Instantiates a PathName object. Required by JPA for entity classes.
     */
    protected PathName() {
        super();
    }

    /**
     * Instantiates a new path name.
     * 
     * @param pName
     *            the pathname
     */
    private PathName(String pName) {
        this.pName = pName;
    }

    /**
     * Returns the pathname in the internal canonical form, which is "/"
     * separated in the manner of Unix paths.
     * 
     * @return the pathname in canonical form
     */
    public String getPathString() {
        return this.pName;
    }

    public int compareTo(Object obj) {
        final PathName other = (PathName) obj;
        return pName.compareTo(other.pName);
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
        return pName.hashCode();
    }

    /**
     * Checks whether the path is a member of its project.
     * 
     * @return true, if the path is a member
     */
    public boolean isMember() {
        // Can't just use File.isAbsolute() because of cross-platform
        // issues. E.g. C:/Foo would appear absolute when the server
        // is running on Windows and relative on Unix.
        final char first = this.pName.charAt(0);
        if (first == '/' || first == '\\') {
            return false;
        } else if (this.pName.charAt(1) != ':') {
            return true;
        } else if (this.pName.matches("^[a-zA-Z]:[/\\\\].*")) { //$NON-NLS-1$
            return false;
        } else {
            return true;
        }
    }

    @Override
    public String toString() {
        return getPathString();
    }

    /**
     * A Builder class for PathName.
     */
    public static class Builder {

        /** The pathname. */
        private final String name;

        /**
         * Instantiates a new builder for PathName.
         * 
         * @param name
         *            the name
         */
        public Builder(String name) {
            this.name = name;
        }

        /**
         * Builds a PathName object.
         * 
         * @return the path name
         */
        public PathName build() {
            return new PathName(this.name);
        }
    }
}
