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

package com.aotool.action;

import java.io.File;
import java.io.IOException;
import java.util.List;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;
import com.aotool.entity.DataService;
import com.aotool.entity.Project;
import com.aotool.entity.Ptx;
import com.aotool.entity.PtxBuilder;
import com.aotool.util.FileDataContainer;
import com.aotool.web.AppProperties;

/**
 * The Admin action implements some client-side access to server administration.
 * Currently this is undocumented and intended only for development. If
 * published it should have some sort of access control. Also, ideally the
 * mapping to it would be hard-wired in the web.xml file so it cannot be
 * subverted or lost.
 */
final class Admin extends GenericAction {

    @Override
    public final String getSummary() {
        return ActionMessages.getString("Admin.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("Admin.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws IOException {

        if (!ah.parseOptions()) {
            return;
        }

        List<String> arglist = ah.getArguments();
        String[] args = arglist.toArray(new String[1]);
        if (args.length == 0 || args[0] == null) {
            ah.errorMessage(ActionMessages.getString("Actions.3")); //$NON-NLS-1$
            return;
        }
        String operation = args[0];

        DataService dataService = ah.getDataService();

        if ("delete".equalsIgnoreCase(operation)) { //$NON-NLS-1$
            if (args.length < 2) {
                ah.errorMessage(ActionMessages.getString("Actions.3")); //$NON-NLS-1$
                return;
            }
            String operand = args[1];
            if ("project".equalsIgnoreCase(operand)) { //$NON-NLS-1$
                for (int i = 2; i < args.length; i++) {
                    String projectName = args[i];
                    File rootDir = AppProperties.getContainerRootDir();
                    if (".".equals(projectName)) { //$NON-NLS-1$
                        dataService.remove(ah.getProject());
                        FileDataContainer.deleteProjectContainers(rootDir, ah.getProject());
                    } else {
                        Project project2 = dataService.find(Project.class, projectName);
                        if (project2 != null) {
                            dataService.remove(project2);
                            FileDataContainer.deleteProjectContainers(rootDir, project2);
                        } else {
                            ah.errorMessage(ActionMessages.getString("Actions.0"), projectName); //$NON-NLS-1$
                        }
                    }
                }
            } else if ("PTX".equalsIgnoreCase(operand)) { //$NON-NLS-1$
                for (int i = 2; i < args.length; i++) {
                    Long ptxId = Ptx.parseIdString(args[i], dataService);
                    PtxBuilder ptxBuilder = new PtxBuilder(dataService, ptxId);
                    ptxBuilder.deletePtx();
                }
            } else if ("ROADMAPDB".equalsIgnoreCase(operand)) { //$NON-NLS-1$
                ah.scrubRoadMap();
            } else if ("DATABASE".equalsIgnoreCase(operand)) { //$NON-NLS-1$
                File rootDir = AppProperties.getContainerRootDir();
                for (Project project2 : dataService.getProjectList()) {
                    dataService.remove(project2);
                    FileDataContainer.deleteProjectContainers(rootDir, project2);
                }
            } else if ("CONTAINERS".equalsIgnoreCase(operand)) { //$NON-NLS-1$
                File rootDir = AppProperties.getContainerRootDir();
                File[] subdirs = rootDir.listFiles();
                for (File subdir : subdirs) {
                    FileDataContainer.removeDir(subdir);
                }
            } else {
                ah.errorMessage(ActionMessages.getString("Actions.2"), operation + ' ' + operand); //$NON-NLS-1$
            }
        } else if ("GC".equalsIgnoreCase(operation)) { //$NON-NLS-1$
            ah.scrubRoadMap();
            if (logger.isInfoEnabled()) {
                logger.info("Forcing garbage collection"); //$NON-NLS-1$
            }
            System.gc();
        } else {
            ah.errorMessage(ActionMessages.getString("Actions.2"), operation); //$NON-NLS-1$
        }
    }
}
