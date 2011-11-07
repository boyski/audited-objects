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
import java.util.LinkedHashMap;
import java.util.List;

import org.incava.util.diff.Difference;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;
import com.aotool.entity.Command;
import com.aotool.entity.CommandAction;
import com.aotool.entity.DataService;
import com.aotool.entity.PathAction;
import com.aotool.util.PathFormat;

/**
 * The DiffCR action is a rough emulation of the ClearCase command of the same
 * name.
 */
final class Diffcr extends GenericAction {

    @Override
    public final String getSummary() {
        return ActionMessages.getString("Diffcr.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("Diffcr.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws IOException {

        ah.registerStandardFlags();

        if (!ah.parseOptions()) {
            return;
        }

        DataService dataService = ah.getDataService();

        String left, right;
        List<String> args = ah.getArguments();
        if (args.size() == 2) {
            left = args.get(0);
            right = args.get(1);
        } else {
            ah.errorMessage(ActionMessages.getString("Diffcr.2")); //$NON-NLS-1$
            return;
        }

        PathAction pathAction_l = PathFormat.findByExtendedName(left, dataService);
        PathAction pathAction_r = PathFormat.findByExtendedName(right, dataService);

        Command command_l = pathAction_l.getCommandAction().getCommand();
        Command command_r = pathAction_r.getCommandAction().getCommand();

        LinkedHashMap<CommandAction, PathAction> actions_l = new LinkedHashMap<CommandAction, PathAction>();
        actions_l.put(pathAction_l.getCommandAction(), pathAction_l);
        if (ah.hasRecursiveFlag() && command_l.equals(command_r)) {
            PathAction.findPredecessors(actions_l, dataService, pathAction_l, ah
                    .hasMembersOnlyFlag());
        }

        LinkedHashMap<CommandAction, PathAction> actions_r = new LinkedHashMap<CommandAction, PathAction>();
        actions_r.put(pathAction_r.getCommandAction(), pathAction_r);
        if (ah.hasRecursiveFlag() && command_l.equals(command_r)) {
            PathAction.findPredecessors(actions_r, dataService, pathAction_r, ah
                    .hasMembersOnlyFlag());
        }

        for (CommandAction ca_l : actions_l.keySet()) {
            Command cmd_l = ca_l.getCommand();
            for (CommandAction ca_r : actions_r.keySet()) {
                Command cmd_r = ca_r.getCommand();
                if (cmd_r.equals(cmd_l)) {
                    diffCommandActions(ca_l, ca_r, ah);
                    List<PathAction> prqs_l = ca_l.getPrereqs();
                    List<PathAction> prqs_r = ca_r.getPrereqs();
                    diffPathActionList(prqs_l, prqs_r, ah);
                    break;
                }
            }
        }
    }

    public final void diffPathActionList(List<PathAction> left, List<PathAction> right,
            ActionHelper ah) throws IOException {

        List<Difference> diffs = new org.incava.util.diff.Diff<PathAction>(left, right,
                PathAction.ORDER_BY_PATH_STATE).diff();
        for (Difference diff : diffs) {
            int delStart = diff.getDeletedStart();
            int delEnd = diff.getDeletedEnd();
            int addStart = diff.getAddedStart();
            int addEnd = diff.getAddedEnd();

            if (delEnd != Difference.NONE) {
                for (int i = delStart; i <= delEnd; i++) {
                    ah.print("< ");
                    PathAction pa = left.get(i);
                    String line = pa.getPathString();
                    ah.println(line);
                }
                if (addEnd != Difference.NONE) {
                    ah.println("---");
                }
            }
            if (addEnd != Difference.NONE) {
                for (int i = addStart; i <= addEnd; i++) {
                    ah.print("> ");
                    PathAction pa = right.get(i);
                    String line = pa.getPathString();
                    ah.println(line);
                }
            }
        }
    }

    public final void diffCommandActions(CommandAction left, CommandAction right, ActionHelper ah)
            throws IOException {

        List<Command> oneCmd_l = new ArrayList<Command>();
        oneCmd_l.add(left.getCommand());
        List<Command> oneCmd_r = new ArrayList<Command>();
        oneCmd_r.add(right.getCommand());
        List<Difference> cmdDiffs = new org.incava.util.diff.Diff<Command>(oneCmd_l, oneCmd_r)
                .diff();
        for (Difference diff : cmdDiffs) {
            int delStart = diff.getDeletedStart();
            int delEnd = diff.getDeletedEnd();
            int addStart = diff.getAddedStart();
            int addEnd = diff.getAddedEnd();

            if (delEnd != Difference.NONE) {
                for (int i = delStart; i <= delEnd; i++) {
                    ah.print("< ");
                    String str = left.getCommand().toStandardString();
                    ah.println(str);
                }
                if (addEnd != Difference.NONE) {
                    ah.println("---");
                }
            }
            if (addEnd != Difference.NONE) {
                for (int i = addStart; i <= addEnd; i++) {
                    ah.print("> ");
                    String str = right.getCommand().toStandardString();
                    ah.println(str);
                }
            }
        }
    }
}
