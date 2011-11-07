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

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.incava.util.diff.Difference;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;
import com.aotool.entity.CommandAction;
import com.aotool.entity.PathAction;
import com.aotool.entity.PathState;
import com.aotool.entity.Ptx;
import com.aotool.util.PathFormat;

/**
 * The Diff action compares results from two PTXes.
 */
final class Diff extends GenericAction {

    @Override
    public final String getSummary() {
        return ActionMessages.getString("Diff.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("Diff.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws IOException {

        ah.registerStandardFlags();

        if (!ah.parseOptions()) {
            return;
        }

        List<Ptx> twoPtxes = ah.getTwoPtxes();
        if (twoPtxes.size() != 2) {
            ah.errorMessage(ActionMessages.getString("Diff.2")); //$NON-NLS-1$
        }
        Ptx leftPtx = twoPtxes.get(0);
        Ptx rightPtx = twoPtxes.get(1);

        List<String> args = ah.getArguments();

        List<PathState> leftList = new ArrayList<PathState>();
        for (PathState ps : getPathStatesUsed(leftPtx, ah)) {
            if (args.size() == 0 || args.contains(ps.getPathString())) {
                leftList.add(ps);
            }
        }
        Collections.sort(leftList);

        List<PathState> rightList = new ArrayList<PathState>();
        for (PathState ps : getPathStatesUsed(rightPtx, ah)) {
            if (args.size() == 0 || args.contains(ps.getPathString())) {
                rightList.add(ps);
            }
        }
        Collections.sort(rightList);

        diffPathStateList(leftList, leftPtx, rightList, rightPtx, ah);
    }

    private Set<PathState> getPathStatesUsed(Ptx ptx, ActionHelper ah) {
        Set<PathState> prereqSet = new HashSet<PathState>();
        Set<PathState> targetSet = new HashSet<PathState>();
        Set<PathState> usedSet = new HashSet<PathState>();
        for (CommandAction ca : ptx.getCommandActions()) {
            for (PathAction pa : ca.getPathActions()) {
                if (ah.hasMembersOnlyFlag() && !pa.isMember()) {
                    continue;
                }

                PathState ps = pa.getPathState();
                if (pa.isTarget()) {
                    if (pa.isUnlink()) {
                        targetSet.remove(ps);
                    } else {
                        targetSet.add(ps);
                    }
                } else {
                    prereqSet.add(ps);
                }
            }
        }
        for (PathState ps : prereqSet) {
            if (!targetSet.contains(ps)) {
                usedSet.add(ps);
            }
        }
        return usedSet;
    }

    public final void diffPathStateList(List<PathState> left, Ptx leftPtx, List<PathState> right,
            Ptx rightPtx, ActionHelper ah) throws IOException {

        List<Difference> diffs = new org.incava.util.diff.Diff<PathState>(left, right).diff();
        for (Difference diff : diffs) {
            int delStart = diff.getDeletedStart();
            int delEnd = diff.getDeletedEnd();
            int addStart = diff.getAddedStart();
            int addEnd = diff.getAddedEnd();

            if (delEnd != Difference.NONE) {
                for (int i = delStart; i <= delEnd; i++) {
                    ah.print("< ");
                    PathState ps = left.get(i);
                    String str = PathFormat.toPrintableString(leftPtx, ps, ah);
                    ah.println(str);
                }
                if (addEnd != Difference.NONE) {
                    ah.println("---");
                }
            }
            if (addEnd != Difference.NONE) {
                for (int i = addStart; i <= addEnd; i++) {
                    ah.print("> ");
                    PathState ps = right.get(i);
                    String str = PathFormat.toPrintableString(rightPtx, ps, ah);
                    ah.println(str);
                }
            }
        }
    }
}
