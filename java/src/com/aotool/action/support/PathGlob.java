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

package com.aotool.action.support;

import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The PathGlob class is used to provide shell-like globbing constructs for
 * choosing pathnames.
 */
public final class PathGlob {

    private final HashSet<Pattern> patterns;

    /**
     * Instantiates a new empty PathGlob object.
     */
    public PathGlob() {
        patterns = new HashSet<Pattern>();
    }

    /**
     * Instantiates a new PathGlob object, given a list of globbing patterns.
     * 
     * @param list
     *            the list
     */
    public PathGlob(List<String> list) {
        this();
        final Iterator<String> iter = list.iterator();
        while (iter.hasNext()) {
            final String s = iter.next();
            final Pattern p = Pattern.compile(s);
            patterns.add(p);
        }
    }

    /**
     * Tests whether this PathGlob matches the supplied path string.
     * 
     * @param path
     *            the pathname
     * 
     * @return true, if successful
     */
    public final boolean matches(String path) {
        if (patterns.size() == 0) {
            return true;
        }
        final Iterator<Pattern> iter = patterns.iterator();
        while (iter.hasNext()) {
            final Pattern p = iter.next();
            final Matcher m = p.matcher(path);
            if (m.find()) {
                return true;
            }
        }
        return false;
    }
}
