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

import static javax.persistence.GenerationType.AUTO;

import java.io.Serializable;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

import javax.persistence.Embedded;
import javax.persistence.Entity;
import javax.persistence.GeneratedValue;
import javax.persistence.Id;
import javax.persistence.ManyToOne;
import javax.persistence.Table;

import com.aotool.Constants;
import com.aotool.Messages;
import com.aotool.util.PathFormat;

/**
 * PathAction is an entity class which represents a particular client-side
 * access (open) of a PathState.
 */
@Entity
@Table(name = "PATH_ACTION_TBL")
public class PathAction implements Serializable {

    private static final long serialVersionUID = 1L;

    /** The primary key id. */
    @Id
    @GeneratedValue(strategy = AUTO)
    private Long id;

    /** The operation code. */
    private char op;

    /** The CommandAction object. */
    @ManyToOne
    private CommandAction commandAction;

    /** The PathState object. */
    @Embedded
    private PathState pathState;

    /**
     * Instantiates a PathAction object. Required by JPA for entity classes.
     */
    protected PathAction() {
        super();
    }

    /**
     * Gets the primary key id.
     * 
     * @return the id
     */
    public Long getId() {
        return this.id;
    }

    /**
     * Gets the operation code.
     * 
     * @return the operation code
     */
    public char getOp() {
        return this.op;
    }

    /**
     * Gets the path state.
     * 
     * @return the path state
     */
    public PathState getPathState() {
        return pathState;
    }

    /**
     * Gets the path on which this action operated in string form.
     * 
     * @return the path name string
     */
    public String getPathString() {
        return this.getPathState().getPathString();
    }

    /**
     * Returns the "standard" string format for this class. The standard format
     * is part of the API and more or less stable, whereas the result of the
     * toString() method is subject to change and best used only for debugging.
     * 
     * @return the standard string format
     */
    public String toStandardString() {
        return PathFormat.format(this);
    }

    @Override
    public String toString() {
        return this.toStandardString();
    }

    /**
     * Sets the command action which created this PathAction.
     * 
     * @param commandAction
     *            the new command action
     */
    public void setCommandAction(CommandAction commandAction) {
        this.commandAction = commandAction;
    }

    /**
     * Gets the command action which created this PathAction.
     * 
     * @return the command action
     */
    public CommandAction getCommandAction() {
        return this.commandAction;
    }

    public Ptx getPtx() {
        return getCommandAction().getPtx();
    }

    /**
     * Checks whether this PathAction is a target (modified).
     * 
     * @return true, if is target
     */
    public boolean isTarget() {
        return op != 'R' && op != 'X';
    }

    /**
     * Checks whether this PathAction is a member of the project.
     * 
     * @return true, if is member
     */
    public boolean isMember() {
        return this.getPathState().getPathName().isMember();
    }

    /**
     * Checks whether this path action is an unlink (remove/delete) operation.
     * 
     * @return true, if unlink
     */
    public boolean isUnlink() {
        return op == 'U';
    }

    /**
     * Checks whether this path action is a symbolic link operation.
     * 
     * @return true, if symlink
     */
    public boolean isSymlink() {
        return op == 'S';
    }

    /**
     * Compare the modification time of this PathAction with that of another.
     * 
     * @param other
     *            the PathAction to compare against
     * 
     * @return -1, 0, or 1
     */
    public int compareMtime(PathAction other) {
        return this.getPathState().compareMtime(other.getPathState());
    }

    /**
     * A comparator for PathActions which considers only the underlying
     * PathState objects.
     */
    public static final Comparator<PathAction> ORDER_BY_PATH_STATE = new Comparator<PathAction>() {
        public int compare(PathAction pa1, PathAction pa2) {
            return pa1.getPathState().compareTo(pa2.getPathState());
        }
    };

    public static void findPredecessors(Map<CommandAction, PathAction> actions,
            DataService dataService, PathAction pathAction, boolean members_only) {
        for (PathAction pa1 : pathAction.getCommandAction().getPrereqs()) {
            if (members_only && !pa1.isMember()) {
                continue;
            }
            Ptx ptx = pa1.getPtx();
            List<PathAction> prevs = PathFormat.getOrderedPathActionListInPtx(pa1.getPathString(),
                    dataService, ptx);
            int size = prevs.size();
            for (int i = 0; i < size; i++) {
                PathAction pa2 = prevs.get(i);
                if (pa2.equals(pa1)) {
                    for (int j = i - 1; j >= 0; j--) {
                        PathAction pa3 = prevs.get(j);
                        if (pa3.isTarget() && !actions.containsKey(pa3.getCommandAction())) {
                            actions.put(pa3.getCommandAction(), pa3);
                            findPredecessors(actions, dataService, pa3, members_only);
                            break;
                        }
                    }
                }
            }
        }
    }

    /**
     * A Builder class for PathAction. Converts a CSV line into a PathAction
     * object.
     */
    public static class Builder {

        /** The number of fields the CSV line is split into. */
        public static final int NUMBER_OF_PATHACTION_FIELDS = 9;

        /** The PathState. */
        private final PathState ps;

        /** The CSV line. */
        private final String csv;

        /**
         * Instantiates a new builder.
         * 
         * @param ps
         *            the path state
         * @param csv
         *            the CSV line
         */
        public Builder(PathState ps, String csv) {
            this.ps = ps;
            this.csv = csv;
        }

        /**
         * Builds a PathAction from the CSV input.
         * 
         * @return the path action
         */
        public PathAction build() {
            final PathAction pa = new PathAction();
            pa.pathState = ps;
            final String[] fields = csv.split(Constants.FS1, NUMBER_OF_PATHACTION_FIELDS);
            if (fields.length == NUMBER_OF_PATHACTION_FIELDS) {
                pa.op = fields[0].charAt(0); // 1
                /*
                 * Fields 2-9 (function, timestamp, pid, depth, ppid, tid,
                 * pccode, ccode) are ignored on the server.
                 */
            } else {
                throw new IllegalArgumentException(Messages.getString("PathAction.0")); //$NON-NLS-1$
            }
            return pa;
        }
    }
}
