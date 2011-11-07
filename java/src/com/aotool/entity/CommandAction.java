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

import static javax.persistence.CascadeType.REMOVE;
import static javax.persistence.GenerationType.AUTO;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import javax.persistence.Column;
import javax.persistence.Embedded;
import javax.persistence.Entity;
import javax.persistence.GeneratedValue;
import javax.persistence.Id;
import javax.persistence.ManyToOne;
import javax.persistence.OneToMany;
import javax.persistence.Table;

import com.aotool.Constants;
import com.aotool.Messages;

/**
 * CommandAction is an entity class which represents a particular invocation of
 * a given Command. A CommandAction references a Command and adds metadata
 * specific to the invocation such as start time, duration, hostname, etc.
 * <p>
 * </p>
 * Thus the relationship between Command and CommandAction is reminiscent of the
 * OO relationship between a class and an object; a CommandAction is a specific
 * instance of a generic Command.
 */
@Entity
@Table(name = "COMMAND_ACTION_TBL")
public class CommandAction implements Serializable, Cloneable, Comparable<Object> {

    private static final long serialVersionUID = 1L;

    /** The primary key id. */
    @Id
    @GeneratedValue(strategy = AUTO)
    private Long id;

    /** The client-side process ID of this invocation */
    private int processID;

    /** The client-side depth of this invocation */
    private int depth;

    /** The client-side process ID of this invocation's parent */
    private int parentProcessID;

    /** The moment that this invocation began. */
    @Embedded
    @Column(name = "START_TIME")
    private Moment startTime;

    /** The duration of this invocation. */
    @Embedded
    private Duration duration;

    /** The name of the PTX from which this CA was recycled, or null. */
    private String recycled;

    /** The Command which was invoked. */
    @Embedded
    private Command command;

    /** The collection of PathActions belonging to this CommandAction. */
    @OneToMany(mappedBy = "commandAction", cascade = REMOVE)
    private Collection<PathAction> pathActionCollection = new ArrayList<PathAction>();

    /** The PTX within which this CA ran. */
    @ManyToOne
    private Ptx ptx;

    /**
     * Instantiates a CommandAction object. Required by JPA for entity classes.
     */
    protected CommandAction() {
        super();
    }

    /**
     * Instantiates a new command action.
     * 
     * @param ptx
     *            the PTX
     * @param command
     *            the Command
     */
    private CommandAction(Ptx ptx, Command command) {
        this.ptx = ptx;
        this.command = command;
    }

    public void addPathAction(PathAction pa) {
        pathActionCollection.add(pa);
        pa.setCommandAction(this);
    }

    public int getProcessID() {
        return processID;
    }

    public int getDepth() {
        return depth;
    }

    public int getParentProcessID() {
        return parentProcessID;
    }

    /**
     * Gets the time stamp when this invocation started.
     * 
     * @return the start time stamp
     */
    public Moment getStartTime() {
        return this.startTime;
    }

    /**
     * Gets the duration of this particular invocation.
     * 
     * @return the duration
     */
    public Duration getDuration() {
        return this.duration;
    }

    /**
     * Gets the name of the host on which this particular invocation ran.
     * 
     * @return the host name
     */
    public String getHostName() {
        /*
         * If we ever support distributed builds we may want to store a hostname
         * per CA but at least for now we can cheat. This value isn't used for
         * anything important anyway.
         */
        return this.ptx.getHostName();
    }

    /**
     * Checks whether this command action was recycled from a previous PTX.
     * 
     * @return true, if recycled
     */
    public boolean isRecycled() {
        return recycled != null && recycled.length() > 0;
    }

    /**
     * If this CA was recycled, returns the PTX in which it originally ran.
     * 
     * @return the recycled-from PTX
     */
    public String getRecycledFromPtxIdString() {
        return recycled;
    }

    /**
     * When a CommandAction has been recycled from a previous PTX, this method
     * will return the original.
     * 
     * @return the CommandAction from which 'this' is recycled
     */
    public CommandAction getCommandActionRecycledFrom(DataService dataService) {
        CommandAction commandAction = null;
        Ptx fromPtx = Ptx.find(dataService, recycled);
        if (fromPtx != null) {
            for (CommandAction ca : fromPtx.getCommandActions()) {
                if (ca.getCommand().equals(this.getCommand())) {
                    commandAction = ca;
                    break;
                }
            }
        }
        return commandAction;
    }

    /**
     * Gets the Command associated with this CommandAction.
     * 
     * @return the command
     */
    public Command getCommand() {
        return this.command;
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
     * Gets the PTX associated with this CommandAction.
     * 
     * @return the PTX
     */
    public Ptx getPtx() {
        return ptx;
    }

    public void setPtx(Ptx ptx) {
        this.ptx = ptx;
    }

    /**
     * Gets the list of prerequisites associated with this CommandAction.
     * 
     * @return the prereq list
     */
    public List<PathAction> getPrereqs() {
        final List<PathAction> prqList = new ArrayList<PathAction>();
        for (final PathAction pa : pathActionCollection) {
            if (!pa.isTarget()) {
                prqList.add(pa);
            }
        }
        return prqList;
    }

    /**
     * Gets the list of targets associated with this CommandAction.
     * 
     * @return the target list
     */
    public List<PathAction> getTargets() {
        final List<PathAction> tgtList = new ArrayList<PathAction>();
        for (final PathAction pa : pathActionCollection) {
            if (pa.isTarget()) {
                tgtList.add(pa);
            }
        }
        return tgtList;
    }

    /**
     * Determines whether this CommandAction wrote to any files.
     * 
     * @return true if one or more target files was written to
     */
    public boolean hasTarget() {
        return !getTargets().isEmpty();
    }

    /**
     * Gets the full collection of PathActions associated with this
     * CommandAction.
     * 
     * @return the collection of path actions
     */
    public Collection<PathAction> getPathActions() {
        return pathActionCollection;
    }

    /**
     * Determines whether this CommandAction is a child of the specified
     * CommandAction
     * 
     * @param other
     *            a CommandAction undergoing a paternity test
     * @return true iff the parameter is the parent
     */
    public boolean isChildOf(CommandAction other) {
        boolean isChild;
        if (other == this) {
            isChild = false;
        } else if (other.getProcessID() == this.getParentProcessID()) {
            isChild = true;
        } else if (other.getProcessID() == this.getProcessID()
                && (other.getDepth() - this.getDepth()) == 1) {
            isChild = true;
        } else {
            isChild = false;
        }
        return isChild;
    }

    public int compareTo(Object o) {
        final CommandAction other = (CommandAction) o;
        final Moment start1 = this.getStartTime();
        final Moment start2 = other.getStartTime();
        final int result = start1.compareTo(start2);
        return result;
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
        final StringBuilder sb = new StringBuilder();
        sb.append(-1).append(Constants.FS1); // Placeholder - field not in use.
        sb.append(-1).append(Constants.FS1); // Placeholder - field not in use.
        sb.append(-1).append(Constants.FS1); // Placeholder - field not in use.
        sb.append(this.getStartTime().toStandardString()).append(Constants.FS1);
        sb.append(this.getDuration().toStandardString()).append(Constants.FS1);
        sb.append(this.getHostName()).append(Constants.FS1);
        sb.append(this.getRecycledFromPtxIdString()).append(Constants.FS1);
        sb.append(this.getCommand().toCSVString());
        return sb.toString();
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
        sb.append(this.toCSVString());
        // sb.append('\n');
        // sb.append(this.getPrereqs().toString()).append('\n');
        // sb.append(this.getTargets().toString());
        return sb.toString();
    }

    @Override
    public String toString() {
        return this.toStandardString();
    }

    /**
     * A Builder class for Command. Converts a CSV line into a Command object.
     */
    public static class Builder {

        /** The number of fields the CSV line is split into. */
        private static final int NUMBER_OF_CMDACTION_FIELDS = 7;

        /** The CSV line. */
        private final String csv;

        /** The Command associated with this CommandAction. */
        private final Command command;

        /** The PTX associated with this CommandAction. */
        private final Ptx ptx;

        /**
         * Instantiates a new builder.
         * 
         * @param ptx
         *            the PTX
         * @param command
         *            the Command
         * @param csv
         *            the CSV line
         */
        public Builder(Ptx ptx, Command command, String csv) {
            this.ptx = ptx;
            this.command = command;
            this.csv = csv;
        }

        /**
         * Builds a CommandAction from the CSV input.
         * 
         * @return the command action
         */
        public CommandAction build() {
            final CommandAction ca = new CommandAction(ptx, command);
            final String[] fields = csv.split(Constants.FS1, NUMBER_OF_CMDACTION_FIELDS);
            if (fields.length == NUMBER_OF_CMDACTION_FIELDS) {
                int field = 0;
                ca.processID = Integer.valueOf(field++);
                ca.depth = Integer.valueOf(field++);
                ca.parentProcessID = Integer.valueOf(field++);
                ca.startTime = new Moment(fields[field++]); // 4
                ca.duration = new Duration(fields[field++]); // 5
                field++; // Field 6 (host) is not currently needed on the server
                ca.recycled = fields[field++]; // 7
            } else {
                throw new IllegalArgumentException(Messages.getString("CommandAction.0")); //$NON-NLS-1$
            }
            return ca;
        }
    }
}
