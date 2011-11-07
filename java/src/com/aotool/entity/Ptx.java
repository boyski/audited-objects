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

import java.io.Serializable;
import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.SimpleTimeZone;

import javax.persistence.AttributeOverride;
import javax.persistence.Basic;
import javax.persistence.Column;
import javax.persistence.Embedded;
import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.ManyToOne;
import javax.persistence.OneToMany;
import javax.persistence.Query;
import javax.persistence.Table;

/**
 * PTX is an entity class representing a Project Transaction, for which it is an
 * abbreviation. This will in most cases translate to a "build", but the PTX
 * terminology is deliberately chosen to be non-build-centric.
 * <p>
 * </p>
 * A PTX corresponds to one explicit (user-invoked) execution of a command
 * within the project. This need not be the "top level" program; in other words,
 * partial builds are allowed. Nor is there a requirement that invoking command
 * lines be identical from PTX to PTX, though opportunities for reuse of audited
 * objects will be correspondingly limited. In short, the audited invocation of
 * <i>any</i> command within the Project tree results in a new PTX for that
 * Project.
 * <p>
 * </p>
 * A PTX has a unique identifier referred to as a <i>PTX ID</i> which serves as
 * a handle to it. An audited object "foo" created by a given PTX is generally
 * referred to as <code>foo@@&lt;ptxid&gt;</code>. This is known as the
 * <i>extended naming</i> format.
 */
@Entity
@Table(name = "PTX_TBL")
public class Ptx implements Serializable, Comparable<Object> {

    private static final long serialVersionUID = 1L;

    /** A date-formatting string used to make the string form of a PTX id. */
    private static final SimpleDateFormat sdf = new SimpleDateFormat("yyyyMMddHHmmss"); //$NON-NLS-1$

    /** The primary key id. */
    @Id
    private Long id;

    /** The base directory. */
    private String baseDirectory;

    /** The relative working directory within which this PTX was initiated. */
    private String relativeWorkingDirectory;

    /** The login name of the user who started this PTX. */
    private String logName;

    /** The group name of the user who started this PTX. */
    private String groupName;

    /** The host name of the client machine. */
    private String hostName;

    /** The system name of the client machine. */
    private String systemName;

    /** The operating system release version of the client machine. */
    private String osRelease;

    /** The type of the client machine. */
    private String machineType;

    /** Indicates whether this PTX is read-only. */
    private boolean readOnly;

    /** The number of file containers uploaded and saved by this PTX. */
    private int uploadedCount;

    /** The number of file containers downloaded by this PTX. */
    private int downloadedCount;

    /** The length of the dataCode used in this PTX, for formatting. */
    private int dataCodeWidth = 8; // use a minimum field width

    /** The result (success, failure, or not-finished). */
    @Basic(optional = true)
    private Boolean result;

    /** The time at which the PTX started from the client perspective. */
    @Embedded
    @AttributeOverride(name = "nanoseconds", column = @Column(name = "START_TIME_CLIENT"))
    private Moment clientStartTime;

    /** The time at which the PTX started from the server perspective. */
    @Embedded
    @AttributeOverride(name = "nanoseconds", column = @Column(name = "START_TIME"))
    private Moment startTime;

    /** The PTX duration. */
    @Embedded
    private Duration duration = new Duration(0);

    /** The project in which this PTX ran. */
    @ManyToOne
    private Project project;

    /**
     * The list of CommandActions which ran in this PTX. Ordering is a tricky
     * issue here. Insertion order is not helpful because commands are generally
     * reported AFTER their children. What we really want is the natural
     * ordering, which is by the time each command started. Currently we keep
     * these in a list in insertion order and re-order the list at retrieval
     * time to achieve the natural order. Perhaps a TreeSet or similar would be
     * more elegant but I have had trouble figuring out how to annotate a
     * TreeSet for persistence and it may not be worth the trouble.
     */
    @OneToMany(mappedBy = "ptx", cascade = REMOVE)
    private List<CommandAction> actionList = new ArrayList<CommandAction>();

    /** An optional label for the PTX. */
    @Basic(optional = true)
    private String label;

    /**
     * Instantiates a PTX object. Required by JPA for entity classes.
     */
    public Ptx() {
        super();
        sdf.setTimeZone(new SimpleTimeZone(0, "GMT")); //$NON-NLS-1$
    }

    /**
     * Gets the primary key id.
     * 
     * @return the id
     */
    public Long getId() {
        return id;
    }

    public void setId(Long id) {
        this.id = id;
    }

    /**
     * Gets the PTX id as a String, formatted as YYYYMMDDHHMMSS in the GMT
     * timezone.
     * 
     * @return the formatted id string
     */
    public String getIdString() {
        final String ptxid = sdf.format(getId());
        return ptxid;
    }

    /**
     * Takes the String-formatted version of a PTX id and returns its integral
     * equivalent in milliseconds.
     * 
     * @param ptxid
     *            a PTX id in String format
     * 
     * @return the same PTX id as a Long
     */
    @SuppressWarnings("unchecked")
    public static Long parseIdString(String ptxid, DataService dataService) {
        Long id = -1L;
        Date parsed = sdf.parse(ptxid, new ParsePosition(0));
        if (parsed != null) {
            id = parsed.getTime();
        } else {
            Query query = dataService.createQuery("" //
                    + " select ptx" //
                    + "   from Ptx ptx" //
                    + "   where ptx.label = :label" //
            );
            query.setParameter("label", ptxid);
            List<Ptx> ptxList = query.getResultList();
            if (!ptxList.isEmpty()) {
                Collections.sort(ptxList);
                Ptx ptx = ptxList.get(0);
                id = ptx.getId();
            }
        }
        return id;
    }

    public static Ptx find(DataService dataService, Long id) {
        Ptx ptx = dataService.find(Ptx.class, id);
        return ptx;
    }

    public static Ptx find(DataService dataService, String idStr) {
        return find(dataService, Ptx.parseIdString(idStr, dataService));
    }

    /**
     * Gets the label.
     * 
     * @return the label
     */
    public String getLabel() {
        return this.label;
    }

    /**
     * Sets the label.
     * 
     * @param label
     *            the new label
     */
    public void setLabel(String label) {
        this.label = label;
    }

    /**
     * Gets a name which can be used to refer to this PTX. If a label is present
     * it is used; otherwise the id string is returned.
     * 
     * @return a name for this PTX
     */
    public String getName() {
        String handle = (label == null) ? getIdString() : getLabel();
        return handle;
    }

    /**
     * Gets the base directory.
     * 
     * @return the base directory
     */
    public String getBaseDirectory() {
        return this.baseDirectory;
    }

    /**
     * Sets the base directory.
     * 
     * @param baseDirectory
     *            the new base directory
     */
    public void setBaseDirectory(String baseDirectory) {
        this.baseDirectory = baseDirectory;
    }

    /**
     * Gets the relative working directory.
     * 
     * @return the relative working directory
     */
    public String getRelativeWorkingDirectory() {
        return this.relativeWorkingDirectory;
    }

    /**
     * Sets the relative working directory.
     * 
     * @param relativeWorkingDirectory
     *            the new relative working directory
     */
    public void setRelativeWorkingDirectory(String relativeWorkingDirectory) {
        this.relativeWorkingDirectory = relativeWorkingDirectory;
    }

    /**
     * Gets the login name of the user which started this PTX.
     * 
     * @return the user name
     */
    public String getLogName() {
        return this.logName;
    }

    /**
     * Sets the login name of the user which started this PTX.
     * 
     * @param logName
     *            the user name
     */
    public void setLogName(String logName) {
        this.logName = logName;
    }

    /**
     * Sets the group name of the user which started this PTX.
     * 
     * @return the group name
     */
    public String getGroupName() {
        return this.groupName;
    }

    /**
     * Sets the group name of the user which started this PTX.
     * 
     * @param groupName
     *            the group name
     */
    public void setGroupName(String groupName) {
        this.groupName = groupName;
    }

    /**
     * Gets the name of the host on which this PTX ran.
     * 
     * @return the host name
     */
    public String getHostName() {
        return this.hostName;
    }

    /**
     * Gets the name of the host on which this PTX ran in unqualified form.
     * 
     * @return the unqualified host name
     */
    public String getUnqualifiedHostName() {
        final int dot = this.hostName.indexOf('.');
        if (dot > 0) {
            return this.hostName.substring(0, dot);
        } else {
            return this.hostName;
        }
    }

    /**
     * Sets the host name reported by the client.
     * 
     * @param hostName
     *            the host name
     */
    public void setHostName(String hostName) {
        this.hostName = hostName;
    }

    /**
     * Gets the system name reported by the client.
     * 
     * @return the system name
     */
    public String getSystemName() {
        return this.systemName;
    }

    /**
     * Sets the system name reported by the client.
     * 
     * @param systemName
     *            the system name
     */
    public void setSystemName(String systemName) {
        this.systemName = systemName;
    }

    /**
     * Gets the OS release reported by the client..
     * 
     * @return the OS release
     */
    public String getOsRelease() {
        return this.osRelease;
    }

    /**
     * Sets the OS release reported by the client.
     * 
     * @param osRelease
     *            the OS release
     */
    public void setOsRelease(String osRelease) {
        this.osRelease = osRelease;
    }

    /**
     * Gets the machine type reported by the client.
     * 
     * @return the machine type
     */
    public String getMachineType() {
        return this.machineType;
    }

    /**
     * Sets the machine type reported by the client.
     * 
     * @param machineType
     *            the new machine type
     */
    public void setMachineType(String machineType) {
        this.machineType = machineType;
    }

    public void addDataCodeWidth(int dataCodeWidth) {
        if (dataCodeWidth > this.dataCodeWidth) {
            this.dataCodeWidth = dataCodeWidth;
        }
    }

    public int getDataCodeWidth() {
        return dataCodeWidth;
    }

    /**
     * Sets the result of this PTX (success or failure).
     * 
     * @param result
     *            true IFF the PTX succeeded
     */
    public void setResult(boolean result) {
        if (this.result == null) {
            this.result = new Boolean(result);
            this.setDuration();
        }
    }

    /*
     * Checks whether this PTX is completed (no longer running).
     * 
     * @return true if closed
     */
    public boolean isDone() {
        return result != null;
    }

    /**
     * Checks whether this PTX is read-only.
     * 
     * @return true if PTX is read-only
     */
    public boolean isReadOnly() {
        return readOnly;
    }

    /**
     * Marks this PTX as being read-only (contributing no uploaded containers)
     * or not.
     */
    public void setReadOnly(boolean readOnly) {
        this.readOnly = readOnly;
    }

    /**
     * Bumps the count of uploaded containers associated with this PTX.
     */
    public synchronized void bumpUploadedCount() {
        this.uploadedCount++;
    }

    /**
     * Returns the number of uploaded containers associated with this PTX.
     * 
     * @return the number of containers associated with this PTX
     */
    public int getUploadedCount() {
        return uploadedCount;
    }

    /**
     * Sets the number of containers downloaded by the PTX.
     */
    public void setDownloadedCount(int downloadedCount) {
        this.downloadedCount = downloadedCount;
    }

    /**
     * Checks whether this PTX has been given a label.
     * 
     * @return true, if labeled
     */
    public boolean isLabeled() {
        return label != null;
    }

    /**
     * Returns the number of downloaded containers associated with this PTX.
     * 
     * @return the number of containers associated with this PTX
     */
    public int getDownloadedCount(DataService dataService) {
        return downloadedCount;
    }

    /**
     * Gets the time when this PTX began, according to the server.
     * 
     * @return the start time stamp
     */
    public Moment getStartTime() {
        return startTime;
    }

    public void setStartTime(Moment value) {
        this.startTime = value;
    }

    /**
     * Gets the duration of this PTX.
     * 
     * @return the duration
     */
    public Duration getDuration() {
        return this.duration;
    }

    /**
     * Sets the duration of this PTX.
     */
    private void setDuration() {
        this.duration = Duration.elapsed(getStartTime());
    }

    /**
     * Gets the time when this PTX began, as provided by the client.
     * 
     * @return the client start time stamp
     */
    public Moment getClientStartTime() {
        return this.clientStartTime;
    }

    /**
     * Sets the time when this PTX began, as provided by the client. There may
     * be time skew between client and server; therefore this is specifically
     * designated as the start time according to the client.
     * 
     * @param clientStartTime
     *            the new client start time stamp
     */
    public void setClientStartTime(Moment clientStartTime) {
        this.clientStartTime = clientStartTime;
    }

    /**
     * Gets the project within which this PTX ran.
     * 
     * @return the project
     */
    public Project getProject() {
        return project;
    }

    /**
     * Associates this PTX with a project.
     * 
     * @param project
     *            the project
     */
    public void setProject(Project project) {
        this.project = project;
    }

    /**
     * Adds a new CommandAction.
     * 
     * @param ca
     *            the CommandAction
     */
    public void addCommandAction(CommandAction ca) {
        actionList.add(ca);
        ca.setPtx(this);
        setDuration();
    }

    /**
     * Gets an ordered list of the CommandAction objects associated with this
     * PTX.
     * 
     * @return the command action list
     */
    public List<CommandAction> getCommandActions() {
        Collections.sort(actionList);
        return actionList;
    }

    /**
     * Gets the command action which is the ancestor of all other command
     * actions within this PTX.
     * 
     * @return the top-level CommandAction
     */
    private CommandAction getTopCommandAction() {
        List<CommandAction> actions = getCommandActions();
        CommandAction top = actions.get(0);
        return top;
    }

    public CommandAction getParentOf(CommandAction current) {
        CommandAction result = null;
        if (current == getTopCommandAction()) {
            result = getTopCommandAction();
        } else {
            for (CommandAction ca : getCommandActions()) {
                if (current.isChildOf(ca)) {
                    result = ca;
                    break;
                }
            }
        }
        return result;
    }

    /**
     * Gets the result string. This is a string representation of the final exit
     * status of the PTX, giving a visual indication of whether it succeeded,
     * failed, or is still in progress.
     * 
     * @return the result string
     */
    public char getResultChar() {
        char resultchar;
        if (result == null) {
            resultchar = '*';
        } else {
            resultchar = result.booleanValue() ? '+' : '-';
        }
        return resultchar;
    }

    /**
     * Returns the "standard" string format for this class. The standard format
     * is part of the API and more or less stable, whereas the result of the
     * toString() method is subject to change and best used only for debugging.
     * 
     * @return the standard string format
     */
    public String toStandardString() {
        return this.getIdString();
    }

    @Override
    public String toString() {
        return toStandardString();
    }

    public int compareTo(Object o) {
        Ptx other = (Ptx) o;
        long cmp = this.getId() - other.getId();
        return cmp > 0 ? 1 : (cmp < 0 ? -1 : 0);
    }
}
