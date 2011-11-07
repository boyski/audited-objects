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

import java.util.Date;

import org.apache.log4j.Logger;

/**
 * 
 * NOTE: All model object passed into methods as parameters are considered to be
 * managed.
 */
public class PtxBuilder {

    private static Long lastPtxIdIssued = 0L;
    private static final Long MILLISECONDS_PER_SECOND = 1000L;

    private static final Logger logger = Logger.getLogger(PtxBuilder.class);

    private DataService dataService;
    private Ptx ptx;

    /**
     * Creates new PTX.
     */
    public PtxBuilder(DataService dataService) {
        this.dataService = dataService;
        ptx = newPtx();
    }

    /**
     * Uses existing PTX.
     */
    public PtxBuilder(DataService dataService, Ptx ptx) {
        this.dataService = dataService;
        this.ptx = ptx;
    }

    /**
     * Uses existing PTX.
     */
    public PtxBuilder(DataService dataService, Long id) {
        this(dataService, id, false);
    }

    public PtxBuilder(DataService dataService, Long id, boolean doLock) {
        this.dataService = dataService;
        if (doLock) {
            this.ptx = dataService.findAndLock(Ptx.class, id);
        } else {
            this.ptx = dataService.find(Ptx.class, id);
        }
    }

    public Ptx getPtx() {
        return ptx;
    }

    private Ptx newPtx() {
        Ptx ptx = new Ptx();
        ptx.setId(getNewPtxId());
        ptx.setStartTime(new Moment());

        dataService.persist(ptx);

        return ptx;
    }

    /**
     * Returns a PTX id value which is guaranteed to be unique. In theory we
     * could arrange things such that different Projects could end up using the
     * same PTX ID, but that turns out to complicate a lot of things. It's far
     * simpler to require them to be globally unique.
     * 
     * @return a value suitable for use as a PTX id
     */
    private static synchronized Long getNewPtxId() {
        Long ptxId = new Date().getTime();
        ptxId -= ptxId % MILLISECONDS_PER_SECOND;
        while (ptxId <= lastPtxIdIssued) {
            ptxId += MILLISECONDS_PER_SECOND;
        }
        lastPtxIdIssued = ptxId;
        if (logger.isDebugEnabled()) {
            logger.debug("Issued new ptxId" + ", value = " + ptxId);
        }
        return ptxId;
    }

    /**
     * Delete a PTX.
     */
    public void deletePtx() {

        ptx.getProject().deletePtx(ptx);
        dataService.remove(ptx);
    }

    public void assignPtxToProject(String projectName) {

        // Start by querying the database to see if the project already
        // exists. If we have an existing project, add the new Ptx to it and
        // merge it. If not, make a new one and persist it. Either way, we
        // now have a fully cooked project containing a Ptx which is just
        // getting started.

        Project project = dataService.find(Project.class, projectName);
        if (project == null) {
            if (logger.isDebugEnabled()) {
                logger.debug("New project" //
                        + ", projectName = " + projectName);
            }
            project = new Project(projectName);
            dataService.persist(project);
        }
        project.addPtx(ptx);
    }

    /**
     * Label a PTX.
     * 
     * @param label
     *            the label
     */
    public void label(String label) {
        ptx.setLabel(label);
    }

    /**
     * Associate a newly created CommandAction object with the PTX within which
     * it ran.
     * 
     * @param commandAction
     *            a new CommandAction to be inserted into the PTX
     */
    public void setCommandAction(CommandAction commandAction) {

        for (PathAction pathAction : commandAction.getPathActions()) {
            dataService.persist(pathAction);
        }
        ptx.addCommandAction(commandAction);
        dataService.persist(commandAction);
    }

    public void setResult(boolean result) {
        ptx.setResult(result);
    }
}
