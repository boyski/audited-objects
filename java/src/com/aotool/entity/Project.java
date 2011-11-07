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
import java.util.ArrayList;
import java.util.List;

import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.NamedQuery;
import javax.persistence.OneToMany;
import javax.persistence.Table;

/**
 * Project is an entity class which represents a coherent tree of related files,
 * such that it can be said to have a single root containing all files belonging
 * to the project and no others. The Project abstraction is necessary in order
 * to be enable comparing like files between users. Imagine that two users have
 * extracted the same tree of files from source control into their respective
 * home directories. We (humans) may know that these are conceptually two
 * 'views' into the same code base but how is a software program to know that
 * the file /home/sean/src/lib/foo.c and /home/lucy/code/src/lib/foo.c are
 * logically the same, and that their corresponding object files in the same
 * directories are potentially interchangeable? The answer is that we place a
 * marker file (which is typically also kept in source control) at the common
 * base of the project. We can then ignore the part of each pathname to the
 * "left" of the marker, with the result that both paths above look like
 * "src/lib/foo.c" for comparison purposes.
 * <p>
 * </p>
 * In the example above we would place a so-called <i>project file</i> into
 * source control such that it would be extracted into /home/sean and
 * /home/lucy/code. Anyone else extracting the same code base will get this
 * project file too and thus everyone's files can be given a canonical
 * ("project-relative") name with respect to that fixed reference point.
 */
@Entity
@NamedQuery(name = "listEveryProject", query = "SELECT p FROM Project p")
@Table(name = "PROJECT_TBL")
public class Project implements Serializable, Comparable<Object> {

    private static final long serialVersionUID = 1L;

    /** The project name. */
    @Id
    private String name;

    /** Holds the PTXes which have taken place within this project. */
    @OneToMany(mappedBy = "project", cascade = REMOVE)
    private List<Ptx> ptxList = new ArrayList<Ptx>();

    /**
     * Instantiates a Project object. Required by JPA for entity classes.
     */
    protected Project() {
        super();
    }

    /**
     * Instantiates a new Project by name.
     * 
     * @param name
     *            the name of the new project
     */
    public Project(String name) {
        this.name = name;
    }

    /**
     * Adds a new PTX to the Project.
     * 
     * @param ptx
     *            the PTX object
     */
    public void addPtx(Ptx ptx) {
        ptxList.add(ptx);
        ptx.setProject(this);
    }

    /**
     * Delete a PTX from the project.
     * 
     * @param ptx
     *            the PTX
     */
    public void deletePtx(Ptx ptx) {
        ptxList.remove(ptx);
        ptx.setProject(null);
    }

    /**
     * Gets the name of the Project.
     * 
     * @return the project name
     */
    public String getName() {
        return this.name;
    }

    /**
     * Gets the collection of known PTXes.
     * 
     * @return the ptx collection
     */
    public List<Ptx> getPtxList() {
        return this.ptxList;
    }

    /**
     * Gets the most recent PTX.
     * 
     * @return the most recent PTX
     */
    public Ptx getMostRecentPtx() {
        Ptx ptx = null;
        List<Ptx> list = getPtxList();
        if (!list.isEmpty()) {
            ptx = list.get(list.size() - 1);
        }
        return ptx;
    }

    /**
     * Checks whether the project is empty. A project which contains PTXes is
     * not empty.
     * 
     * @return true, if empty
     */
    public boolean isEmpty() {
        return ptxList.isEmpty();
    }

    /**
     * Returns the "standard" string format for this class. The standard format
     * is part of the API and more or less stable, whereas the result of the
     * toString() method is subject to change and best used only for debugging.
     * 
     * @return the standard string format
     */
    public String toStandardString() {
        return name;
    }

    @Override
    public String toString() {
        return this.toStandardString();
    }

    public int compareTo(Object obj) {
        final Project other = (Project) obj;
        return this.getName().compareTo(other.getName());
    }
}
