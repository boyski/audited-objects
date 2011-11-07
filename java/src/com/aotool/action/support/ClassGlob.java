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

import java.io.File;
import java.io.FilenameFilter;

/**
 * The ClassGlob class is a helper class for matching command name
 * abbreviations, in other words allowing "lsb" as a shorthand for "lsbuilds".
 */
public final class ClassGlob implements FilenameFilter {

    private final String regex;

    /**
     * Instantiates a new class glob.
     * 
     * @param dir
     *            the dir
     * @param glob
     *            the glob
     */
    public ClassGlob(String dir, String glob) {
        this.regex = dir.toLowerCase() + glob;
    }

    public boolean accept(File fir, String name) {
        return name.toLowerCase().matches(regex);
    }
}
