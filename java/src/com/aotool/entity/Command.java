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

import javax.persistence.Embeddable;
import javax.persistence.Lob;

import com.aotool.Constants;

/**
 * Command is an entity class representing a command which was run on a client
 * machine. A Command is defined to be a particular textual command line which
 * operates on a particular set of files.
 * <p>
 * </p>
 * The first part of this, the textual command line, is easy. However, the same
 * command line may well recur at various points in the build tree with
 * different semantics. For instance,a project might have a convention requiring
 * a UnitTest.cpp file in every directory. Assuming the same compiler flags are
 * used across the board, each of these will be compiled by an identical command
 * line but will generate different object files containing different bits.
 * <p>
 * </p>
 * The obvious answer would be to use the current working directory as a further
 * uniqifier. Unfortunately that would be a bad idea, because while some
 * commands behave differently depending on their CWD, others do not. The Unix
 * command <code>/bin/cp /tmp/foo /tmp/bar</code>, for instance, touches the
 * same files regardless of CWD. In practice, it's not possible to determine by
 * inspection which textual command lines are in which category. Therefore, the
 * current working directory is not allowed to participate in uniqueness
 * determination.
 * <p>
 * </p>
 * Instead of the CWD we use the <i>path code</i>. This is essentially a hash of
 * the absolute paths of all files opened by that Command (not the contents,
 * just the names). Two textually identical commands which address their files
 * via relative paths, such as <code>cp foo bar</code>, will have different
 * pathcodes when run in different directories because the <i>absolute</i> paths
 * of those files will differ.
 * <p>
 * </p>
 * Thus the combination of command line and pathcode is considered sufficient
 * evidence that two Commands are actually the same. The only problem with this
 * technique is that, unlike CWD, it cannot be determined until after the
 * Command has run.
 */
@Embeddable
public class Command implements Comparable<Object>, Serializable {

    private static final long serialVersionUID = 1L;

    private static final int NUMBER_OF_CMD_FIELDS = 6;

    private static final String CSV_NEWLINE_TOKEN = "^J";

    @Lob
    private String cmdbuf;

    private boolean aggregated;

    private transient String[] fields;

    /**
     * Instantiates a Command object. Required by JPA for entity classes.
     */
    protected Command() {
        super();
    }

    /**
     * Gets the name of the program involved in this command.
     * 
     * @return the program name
     */
    public String getProgram() {
        return getField(0);
    }

    /**
     * Gets the relative working directory in which the command took place.
     * 
     * @return the relative working directory
     */
    public String getRelativeWorkingDirectory() {
        return getField(1);
    }

    /**
     * Gets the "command code" of this command's parent.
     * 
     * @return the pccode
     */
    public String getPccode() {
        return getField(2);
    }

    /**
     * Checks for the presence of a parent command code.
     * 
     * @return true, if this command has a parent command code
     */
    private boolean hasPccode() {
        return this.getPccode() != null;
    }

    /**
     * Gets the "command code" of this command.
     * 
     * @return the ccode
     */
    public String getCcode() {
        return getField(3);
    }

    /**
     * Gets the "path code" of this command.
     * 
     * @return the pathcode
     */
    public String getPathCode() {
        return getField(4);
    }

    public boolean hasPathCode() {
        return this.getPathCode() != null;
    }

    /**
     * Gets the full command line as seen by exec() or CreateProcess(), modified
     * to contain no newlines.
     * 
     * @return the command line
     */
    public String getLine() {
        return getField(5);
    }

    /**
     * Gets the full command line as seen by exec() or CreateProcess(),
     * including original newlines.
     * 
     * @return the command line
     */
    public String getOriginalLine() {
        String line = getLine();
        String nline = line.replace(CSV_NEWLINE_TOKEN, "\n");
        return nline;
    }

    /**
     * Checks whether this command has aggregated children.
     * 
     * @return true, if aggregated
     */
    public boolean isAggregated() {
        return aggregated;
    }

    /**
     * Marks this command as being aggregated.
     */
    public void setAggregated() {
        this.aggregated = true;
    }

    private String getField(int index) {
        String field = null;
        if (fields == null) {
            fields = this.cmdbuf.split(Constants.FS1, NUMBER_OF_CMD_FIELDS);
        }
        if (index < fields.length && !Constants.CSV_NULL_FIELD.equals(fields[index])) {
            field = fields[index];
        }
        return field;
    }

    /**
     * Stringifies this object in its standard CSV format.
     * 
     * @return a CSV line representing this command
     */
    public String toCSVString() {
        return cmdbuf;
    }

    /**
     * Returns the "standard" string format for this class. The standard format
     * is part of the API and more or less stable, whereas the result of the
     * toString() method is subject to change and best used only for debugging.
     * 
     * @return the standard string format
     */
    public String toStandardString() {
        return this.toCSVString();
    }

    @Override
    public String toString() {
        return this.toStandardString();
    }

    /*
     * We need some kind of ordering for Command objects but I don't believe it
     * matters so much what it is. On both server and client, we treat the
     * combination of cmdline and pathcode as dispositive in terms of command
     * equality. We used to consider depth too but that breaks in a recursive
     * build if the user cd's down a level because depth is relative. Update: we
     * now use pccode too but only if present for both objects. The parent ccode
     * is subject to the same problem as depth, but it turns out we need to
     * consider it here as long as we also consider it in shopping. Perhaps the
     * better answer is to try to find an improvement on pccode? There's
     * currently an ugly hack whereby we return {2,-2} instead of {1,-1} when
     * only pccode differs. TODO - rethink!
     */
    public int compareTo(Object obj) {
        final Command other = (Command) obj;
        int result;
        if (this == other) {
            result = 0;
        } else {
            final String this_line = this.getLine();
            final String that_line = other.getLine();
            if (this_line == null || that_line == null) {
                if (this_line == that_line) {
                    result = 0;
                } else {
                    result = that_line == null ? 1 : -1;
                }
            } else {
                final int diff = this_line.compareTo(that_line);
                if (diff != 0) {
                    result = diff < 0 ? -1 : 1;
                } else {
                    if (this.hasPathCode() && other.hasPathCode()) {
                        result = this.getPathCode().compareTo(other.getPathCode()) < 0 ? -1 : 1;
                    } else {
                        result = 0;
                    }

                    /*
                     * By this point we've tested everything that affects actual
                     * equality. The following is just an ordering hack.
                     */
                    if (result == 0 && this.hasPccode() && other.hasPccode()) {
                        int pccdiff = this.getPccode().compareTo(other.getPccode());
                        result = pccdiff < 0 ? -2 : (pccdiff > 0) ? 2 : 0;
                    }
                }
            }
        }
        return result;
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
            // HACK - for now at least we have a hack in which pccode
            // is used for Command ordering but ignored for equality,
            // which we handle by returning 2 or -2 instead of 1 or -1.
            // This is awful and maybe broken in some way but TBD.
            int xdiff = this.compareTo(obj);
            if (xdiff == -1 || xdiff == 1) {
                return false;
            } else {
                return true;
            }
        }
    }

    @Override
    public final int hashCode() {
        // Remember to stay "consistent with equals" here.
        int result = 17;
        final String line = this.getLine();
        if (line != null) {
            result = (result * 37) + line.hashCode();
        }
        final String pathcode = this.getPathCode();
        if (pathcode != null) {
            result = (result * 37) + pathcode.hashCode();
        }
        return result;
    }

    /**
     * A Builder class for Command. Converts a CSV line into a Command object.
     */
    public static class Builder {

        private final String csv;

        /**
         * Instantiates a new builder.
         * 
         * @param csv
         *            the csv line
         */
        public Builder(String csv) {
            this.csv = csv;
        }

        /**
         * Builds a Command from the CSV input.
         * 
         * @return the command
         */
        public Command build() {
            final Command cmd = new Command();
            cmd.cmdbuf = csv;
            return cmd;
        }
    }
}
