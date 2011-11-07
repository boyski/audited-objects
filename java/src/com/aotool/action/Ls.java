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
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.PatternSyntaxException;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;
import com.aotool.action.support.PathGlob;
import com.aotool.entity.CommandAction;
import com.aotool.entity.Duration;
import com.aotool.entity.PathAction;
import com.aotool.entity.PathState;
import com.aotool.entity.Ptx;
import com.aotool.util.FileDataContainer;
import com.aotool.util.PathFormat;
import com.aotool.web.AppProperties;

/**
 * The Ls action is used to list all files used by a given build.
 */
final class Ls extends GenericAction {

    private final static String TARGETS_FLAG = ActionMessages.getString("Flags.3"); //$NON-NLS-1$
    private final static String PREREQS_FLAG = ActionMessages.getString("Flags.4"); //$NON-NLS-1$
    private final static String COMMANDS_FLAG = ActionMessages.getString("Flags.5"); //$NON-NLS-1$
    private final static String SOURCES_FLAG = ActionMessages.getString("Flags.17"); //$NON-NLS-1$
    private final static String UPLOADS_FLAG = ActionMessages.getString("Flags.18"); //$NON-NLS-1$
    private final static String DOWNLOADS_FLAG = ActionMessages.getString("Flags.19"); //$NON-NLS-1$

    @Override
    public final String getSummary() {
        return ActionMessages.getString("Ls.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("Ls.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws IOException {

        ah.registerStandardFlags();
        ah.registerFlagOption('T', TARGETS_FLAG, ActionMessages.getString("Ls.2")); //$NON-NLS-1$
        ah.registerFlagOption('P', PREREQS_FLAG, ActionMessages.getString("Ls.3")); //$NON-NLS-1$
        ah.registerFlagOption('C', COMMANDS_FLAG, ActionMessages.getString("Ls.4")); //$NON-NLS-1$
        ah.registerFlagOption('S', SOURCES_FLAG, ActionMessages.getString("Ls.5")); //$NON-NLS-1$
        ah.registerFlagOption('U', UPLOADS_FLAG, ActionMessages.getString("Ls.6")); //$NON-NLS-1$
        ah.registerFlagOption('D', DOWNLOADS_FLAG, ActionMessages.getString("Ls.7")); //$NON-NLS-1$

        if (!ah.parseOptions()) {
            return;
        }

        boolean long_flag = ah.hasLongFlag();
        boolean short_flag = ah.hasShortFlag();
        boolean members_only_flag = ah.hasMembersOnlyFlag();
        boolean targets_flag = ah.hasFlag(TARGETS_FLAG);
        boolean prereqs_flag = ah.hasFlag(PREREQS_FLAG);
        boolean commands_flag = ah.hasFlag(COMMANDS_FLAG);
        boolean sources_flag = ah.hasFlag(SOURCES_FLAG);
        boolean uploads_flag = ah.hasFlag(UPLOADS_FLAG);
        boolean downloads_flag = ah.hasFlag(DOWNLOADS_FLAG);

        Ptx ptx = ah.getPtx();

        if (ptx == null) {
            ah.noSuchPtxError();
            return;
        }

        List<String> arglist = ah.getArguments();
        PathGlob glob;
        try {
            glob = new PathGlob(arglist);
        } catch (PatternSyntaxException e) {
            ah.errorMessage((e.getMessage()));
            return;
        }

        Set<PathState> printSet = new HashSet<PathState>();

        if (commands_flag) {
            for (CommandAction ca : ptx.getCommandActions()) {
                /*
                 * Only certain flag combinations make sense. Arguably these
                 * should be handled via error messages instead.
                 */
                if (!targets_flag && !prereqs_flag && !short_flag) {
                    targets_flag = prereqs_flag = true;
                }

                if (short_flag) {
                    if (!targets_flag || ca.hasTarget()) {
                        ah.println(ca.getCommand().getOriginalLine());
                    }
                    continue;
                }

                StringBuilder linesb = new StringBuilder();
                linesb.append('\n');
                linesb.append('*');
                if (long_flag) {
                    linesb.append('[');
                    Duration duration = ca.getDuration();
                    linesb.append(duration.toSimpleString());
                    linesb.append(']');
                }
                linesb.append(' ');
                linesb.append(ca.getCommand().getOriginalLine());
                ah.println(linesb.toString());

                if (ca.isRecycled()) {
                    ah.println(ActionMessages.getString("Ls.8")); //$NON-NLS-1$
                    continue;
                }

                for (PathAction pa : ca.getPathActions()) {
                    if (members_only_flag && !pa.isMember()) {
                        continue;
                    } else if (!targets_flag && pa.isTarget()) {
                        continue;
                    } else if (!prereqs_flag && !pa.isTarget()) {
                        continue;
                    } else if (!glob.matches(pa.getPathString())) {
                        continue;
                    }

                    if (long_flag) {
                        ah.print(pa.getOp());
                    } else {
                        if (!short_flag) {
                            if (pa.isTarget()) {
                                if (pa.isUnlink()) {
                                    ah.print('-');
                                } else {
                                    ah.print('>');
                                }
                            } else {
                                ah.print(' ');
                            }
                        }
                        ah.print(' ');
                    }

                    String str = PathFormat.toPrintableString(pa, ah);
                    ah.println(str);
                }
                ah.print('\n');
            }
        } else if (uploads_flag || downloads_flag) {
            File rootDir = AppProperties.getContainerRootDir();
            for (CommandAction ca : ptx.getCommandActions()) {
                if (ca.isRecycled()) {
                    if (downloads_flag) {
                        CommandAction ca1 = ca.getCommandActionRecycledFrom(ah.getDataService());
                        if (ca1 != null) {
                            ca = ca1;
                        } else {
                            ah.errorMessage(ActionMessages.getString("Ls.9") //$NON-NLS-1$
                                    + ca.getRecycledFromPtxIdString(), ca.getCommand().getLine());
                            continue;
                        }
                    } else {
                        continue;
                    }
                } else {
                    if (downloads_flag) {
                        continue;
                    }
                }
                for (PathAction pa : ca.getTargets()) {
                    PathState ps = pa.getPathState();
                    if (printSet.contains(ps)) {
                        continue;
                    } else if (members_only_flag && !pa.isMember()) {
                        continue;
                    }
                    FileDataContainer container = new FileDataContainer(rootDir, ah.getProject(),
                            ps);
                    if (!container.exists()) {
                        continue;
                    }
                    printSet.add(ps);
                }
            }
            List<PathState> printList = new ArrayList<PathState>(printSet);
            printSet = null; // GC hint
            Collections.sort(printList);
            for (PathState ps : printList) {
                String str = PathFormat.toPrintableString(ptx, ps, ah);
                ah.println(str);
            }
        } else {
            /*
             * Only certain flag combinations make sense. Arguably these should
             * be handled via error messages instead.
             */
            if (sources_flag) {
                targets_flag = prereqs_flag = commands_flag = false;
            } else if (!targets_flag && !prereqs_flag) {
                targets_flag = prereqs_flag = true;
            }

            Set<PathState> prereqSet = new HashSet<PathState>();
            Set<PathState> targetSet = new HashSet<PathState>();
            for (CommandAction ca : ptx.getCommandActions()) {
                for (PathAction pa : ca.getPathActions()) {
                    if (members_only_flag && !pa.isMember()) {
                        continue;
                    } else if (!glob.matches(pa.getPathString())) {
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

            if (sources_flag) {
                /*
                 * If a pathname was EVER seen as a prereq and NEVER seen as a
                 * target, then we assume it came from source control and is
                 * required for the build.
                 */
                for (PathState ps : prereqSet) {
                    if (!targetSet.contains(ps)) {
                        printSet.add(ps);
                    }
                }
            } else {
                if (prereqs_flag) {
                    for (PathState ps : prereqSet) {
                        printSet.add(ps);
                    }
                }
                if (targets_flag) {
                    for (PathState ps : targetSet) {
                        printSet.add(ps);
                    }
                }
            }
            prereqSet = null; // GC hint
            List<PathState> printList = new ArrayList<PathState>(printSet);
            printSet = null; // GC hint
            Collections.sort(printList);
            for (PathState ps : printList) {
                String str = PathFormat.toPrintableString(ptx, ps, ah);
                if (!short_flag && !long_flag) {
                    if (targetSet.contains(ps)) {
                        ah.print('>');
                    } else {
                        ah.print('<');
                    }
                    ah.print(' ');
                }
                ah.println(str);
            }
        }
    }
}
