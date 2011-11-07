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
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;
import com.aotool.entity.PathAction;
import com.aotool.entity.PathState;
import com.aotool.util.FileDataContainer;
import com.aotool.util.PathFormat;
import com.aotool.web.AppProperties;

/**
 * Lsdo is a rough emulation of the ClearCase command of the same name.
 */
final class Lsdo extends GenericAction {

    @Override
    public final String getSummary() {
        return ActionMessages.getString("Lsdo.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("Lsdo.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws IOException {

        ah.registerStandardFlags();

        if (!ah.parseOptions()) {
            return;
        }

        // No sense printing naked pathnames here.
        ah.setExtendedNamesFlag(true);

        File rootDir = AppProperties.getContainerRootDir();

        Set<String> seen = new HashSet<String>();
        for (String arg : ah.getArguments()) {
            List<PathAction> paList = ah.getOrderedPathActionListByPath(arg);
            if (paList.isEmpty()) {
                ah.errorMessage(ActionMessages.getString("Lsdo.2"), arg); //$NON-NLS-1$
                continue;
            }
            for (PathAction pa : paList) {
                PathState ps = pa.getPathState();
                FileDataContainer container = new FileDataContainer(rootDir, ah.getProject(), ps);
                String containerPath = container.toStandardString();
                if (seen.contains(containerPath) || !container.exists()) {
                    continue;
                }
                seen.add(containerPath);
                String str = PathFormat.toPrintableString(pa, ah);
                ah.println(str);
            }
        }
    }
}
