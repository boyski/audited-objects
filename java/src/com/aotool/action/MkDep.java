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
import java.util.Iterator;
import java.util.List;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;
import com.aotool.action.support.PathGlob;
import com.aotool.entity.CommandAction;
import com.aotool.entity.PathAction;
import com.aotool.entity.PathName;
import com.aotool.entity.Ptx;
import com.aotool.util.PathFormat;

/**
 * The MkDep action is used to dump make-style prerequisite data.
 */
final class MkDep extends GenericAction {

    @Override
    public final String getSummary() {
        return ActionMessages.getString("MkDep.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("MkDep.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws IOException {

        ah.registerStandardFlags();

        if (!ah.parseOptions()) {
            return;
        }

        boolean members_only = ah.hasMembersOnlyFlag();

        Ptx ptx = ah.getPtx();

        String base = ah.hasAbsoluteFlag() ? ptx.getBaseDirectory() : null;
        boolean dosflag = ah.clientIsWindows();

        if (ptx == null) {
            ah.noSuchPtxError();
            return;
        }

        List<String> arglist = ah.getArguments();
        PathGlob glob = new PathGlob(arglist);

        List<CommandAction> caList = ptx.getCommandActions();
        Iterator<CommandAction> caIter = caList.iterator();
        while (caIter.hasNext()) {

            CommandAction ca = caIter.next();
            ArrayList<PathName> dumpList = new ArrayList<PathName>();
            StringBuilder sb = new StringBuilder();

            /*
             * First dump out the targets, formatted neatly, followed by the
             * prereqs. Each set must be traversed twice, once to winnow out the
             * ones we don't want to print and then again to actually print the
             * ones we do. It's done this way because we need to identify the
             * last one in each list for formatting purposes.
             */
            for (PathAction pa : ca.getTargets()) {
                if (!pa.isUnlink() && glob.matches(pa.getPathString())) {
                    dumpList.add(pa.getPathState().getPathName());
                }
            }
            for (int i = 0; i < dumpList.size(); i++) {
                PathName pn = dumpList.get(i);
                String path = PathFormat.getNativePathString(pn, base, dosflag);
                sb.append(path);
                if (i < dumpList.size() - 1) {
                    sb.append(" \\\n"); //$NON-NLS-1$
                }
            }
            dumpList.clear();

            // If the command has no targets, skip the whole thing.
            if (sb.length() == 0) {
                continue;
            }

            sb.append(": \\\n"); //$NON-NLS-1$

            for (PathAction pa : ca.getPrereqs()) {
                if (!members_only || pa.isMember()) {
                    dumpList.add(pa.getPathState().getPathName());
                }
            }

            // Skip if no prereqs.
            if (dumpList.isEmpty()) {
                continue;
            }

            for (int i = 0; i < dumpList.size(); i++) {
                PathName pn = dumpList.get(i);
                String path = PathFormat.getNativePathString(pn, base, dosflag);
                sb.append("    "); //$NON-NLS-1$
                sb.append(path);
                if (i < dumpList.size() - 1) {
                    sb.append(" \\\n"); //$NON-NLS-1$
                }
            }
            dumpList.clear();

            if (caIter.hasNext()) {
                sb.append('\n');
            }

            ah.println(sb.toString());
        }
    }
}
