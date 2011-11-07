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

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;

/**
 * The About action prints copyrights and licenses to the user in plain-text
 * format. Some of these are required by license terms while others are
 * optional. However, the general policy is to give credit where due and thus to
 * show all license and copyright data.
 */
final class About extends GenericAction {

    @Override
    public final String getSummary() {
        return ActionMessages.getString("About.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("About.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws FileNotFoundException, IOException {

        if (!ah.parseOptions()) {
            return;
        }

        String base = ah.getRealPath("/"); //$NON-NLS-1$
        File licfile = new File(base, "LICENSES.txt"); //$NON-NLS-1$
        FileReader fr = new FileReader(licfile);
        BufferedReader br = new BufferedReader(fr);
        String line;
        while ((line = br.readLine()) != null) {
            ah.println(line);
        }
        br.close();
    }
}
