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
import java.util.LinkedHashMap;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;
import com.aotool.entity.CommandAction;
import com.aotool.entity.DataService;
import com.aotool.entity.PathAction;
import com.aotool.entity.Ptx;
import com.aotool.util.PathFormat;

/**
 * The CatCR action is a rough emulation of the ClearCase command of the same
 * name.
 */
final class Catcr extends GenericAction {

    @Override
    public final String getSummary() {
        return ActionMessages.getString("Catcr.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("Catcr.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws IOException {

        ah.registerStandardFlags();
        ah.registerRecursiveFlag();

        if (!ah.parseOptions()) {
            return;
        }

        DataService dataService = ah.getDataService();

        for (String arg : ah.getArguments()) {

            PathAction pathAction = PathFormat.findByExtendedName(arg, dataService);
            if (pathAction == null) {
                ah.errorMessage(ActionMessages.getString("Catcr.8"), arg); //$NON-NLS-1$
                continue;
            }

            // We need a map type which remembers insertion order.
            LinkedHashMap<CommandAction, PathAction> actions = new LinkedHashMap<CommandAction, PathAction>();

            // Always start with the named PA ...
            actions.put(pathAction.getCommandAction(), pathAction);

            // ... then recurse into predecessors if requested.
            if (ah.hasRecursiveFlag()) {
                PathAction.findPredecessors(actions, dataService, pathAction, ah
                        .hasMembersOnlyFlag());
            }

            for (CommandAction ca : actions.keySet()) {
                catcr(ah, ca, actions.get(ca));
            }
        }
    }

    private void catcr(ActionHelper ah, CommandAction commandAction, PathAction pathAction)
            throws IOException {
        Ptx ptx = commandAction.getPtx();
        String user = ptx.getLogName();
        String group = ptx.getGroupName();
        String basedir = ptx.getBaseDirectory();
        String base = ah.hasAbsoluteFlag() ? basedir : null;
        if (!ah.hasShortFlag()) {
            ah.printf("Audited Object: %s\n", PathFormat.format(pathAction));
            ah.printf("Target %s built by %s.%s\n", PathFormat.getNativePathString(pathAction,
                    base, ah.clientIsWindows()), user, group);
            ah.printf("Host \"%s\" running %s %s (%s)\n", ptx.getHostName(), ptx.getSystemName(),
                    ptx.getOsRelease(), ptx.getMachineType());
            ah.printf("Reference Time %s, this audit started %s\n", ptx.getClientStartTime()
                    .toISOStyleString(), commandAction.getStartTime().toISOStyleString());
            ah.printf("View was %s:%s\n", ptx.getHostName(), basedir);
            ah.printf("Initial working directory was %s\n", basedir);
        }
        ah.println(BARS);
        ah.println("Members:");
        ah.println(BARS);
        for (PathAction pa : commandAction.getPathActions()) {
            if (!pa.isMember()) {
                continue;
            }
            String str = PathFormat.toPrintableString(pa, ah);
            ah.println(str);
        }
        ah.println(BARS);
        if (!ah.hasMembersOnlyFlag()) {
            ah.println("Non-members:");
            ah.println(BARS);
            for (PathAction pa : commandAction.getPathActions()) {
                if (pa.isMember()) {
                    continue;
                }
                String str = PathFormat.toPrintableString(pa, ah);
                ah.println(str);
            }
        }
        if (!ah.hasShortFlag()) {
            ah.println(BARS);
            ah.println("Build script:");
            ah.println(BARS);
            ah.print('\t');
            ah.println(commandAction.getCommand().getOriginalLine());
            ah.println(BARS);
        }
    }
}
