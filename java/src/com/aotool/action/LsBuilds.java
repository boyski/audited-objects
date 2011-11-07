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

import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;
import com.aotool.entity.CommandAction;
import com.aotool.entity.Duration;
import com.aotool.entity.PathAction;
import com.aotool.entity.PathState;
import com.aotool.entity.Ptx;
import com.aotool.web.Http;

/**
 * The LsBuilds action is used to list the audited builds of a project.
 */
final class LsBuilds extends GenericAction {

    //private static final String lsfmt_r = "%-14s %c %-8s %-5s %-8s %-10s"; //$NON-NLS-1$
    private static final String lsfmt_l = "%-14s %c %-8s %-5s %-5s %-8s %-10s"; //$NON-NLS-1$

    @Override
    public final String getSummary() {
        return ActionMessages.getString("LsBuilds.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("LsBuilds.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws FileNotFoundException, IOException {

        ah.registerLongFlag();
        ah.registerShortFlag();

        if (!ah.parseOptions()) {
            return;
        }

        List<Ptx> ptxList = new ArrayList<Ptx>();
        String idStr = ah.getIDOption();
        if (idStr != null) {
            Ptx ptx = ah.getPtx(idStr);
            if (ptx != null) {
                ptxList.add(ptx);
            }
        } else {
            ptxList.addAll(ah.getPtxList());
        }

        if (ptxList.isEmpty()) {
            return;
        }

        Set<PathState> filterSet = new HashSet<PathState>();
        for (String csv : ah.getHeaders(Http.X_PATHSTATE_HEADER)) {
            PathState ps = new PathState.Builder(csv).build();
            filterSet.add(ps);
        }

        List<Ptx> printList;
        if (filterSet.isEmpty()) {
            printList = ptxList;
        } else {
            printList = new ArrayList<Ptx>();
            Iterator<Ptx> ptxIter = ptxList.iterator();
            ptxloop: while (ptxIter.hasNext()) {
                Ptx ptx = ptxIter.next();
                Set<PathState> tgtSet = new HashSet<PathState>();
                for (CommandAction ca : ptx.getCommandActions()) {
                    for (PathAction tgt : ca.getTargets()) {
                        tgtSet.add(tgt.getPathState());
                    }
                }
                for (PathState ps : filterSet) {
                    if (tgtSet.contains(ps)) {
                        printList.add(ptx);
                        continue ptxloop;
                    }
                }
            }
        }

        if (printList.isEmpty()) {
            return;
        }

        if (!ah.hasShortFlag()) {
            ah.printf(lsfmt_l, "PTX", '?', "DURATION", "UP", "DOWN", "USER", "HOST"); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$ //$NON-NLS-6$
            ah.printf(" %s", "LABEL"); //$NON-NLS-1$ //$NON-NLS-2$
            ah.println();
        }

        for (Ptx ptx : printList) {
            lsbuilds(ah, ptx);
        }
    }

    public static void lsbuilds(ActionHelper ah, Ptx ptx) throws IOException {
        String ptxIdString = ptx.getIdString().toString();
        if (ah.hasShortFlag()) {
            ah.print(ptxIdString);
        } else {
            Duration duration = ptx.getDuration();
            String durstr = duration.toStandardString();
            String upstr = Integer.toString(ptx.getUploadedCount());
            String downstr = Integer.toString(ptx.getDownloadedCount(ah.getDataService()));
            ah.printf(lsfmt_l, ptxIdString, ptx.getResultChar(), durstr, upstr, downstr, ptx
                    .getLogName(), ptx.getUnqualifiedHostName());
            String label = ptx.getLabel();
            if (label != null) {
                ah.printf(" %s", label); //$NON-NLS-1$
            }
        }
        ah.println();
    }

}
