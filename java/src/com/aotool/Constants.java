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

package com.aotool;

/**
 * The Constants class holds a few constants of global interest.
 */
public final class Constants {

    /** The radix in which numbers are represented in CSV lines. */
    public static final int CSV_RADIX = 36; // Character.MAX_RADIX;

    /** Represents a null (as opposed to empty) CSV field. */
    public static final String CSV_NULL_FIELD = "-"; //$NON-NLS-1$

    /** The field separator for comma-separated-value lines. */
    public static final String FS1 = ","; //$NON-NLS-1$

    /** The separator for compound values within a CSV line. */
    public static final String FS2 = "+"; //$NON-NLS-1$
}
