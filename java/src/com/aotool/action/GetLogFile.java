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

import java.io.BufferedInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.zip.GZIPInputStream;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.GenericAction;
import com.aotool.entity.Ptx;
import com.aotool.util.FileDataContainer;
import com.aotool.web.AppProperties;
import com.aotool.web.Http;

/**
 * The GetLogFile action is used to get the logfile for a given build.
 */
final class GetLogFile extends GenericAction {

    @Override
    public final String getSummary() {
        return ActionMessages.getString("GetLogFile.0"); //$NON-NLS-1$
    }

    @Override
    public final String getHelpMessage() {
        return ActionMessages.getString("GetLogFile.1"); //$NON-NLS-1$
    }

    @Override
    public void doAction(ActionHelper ah) throws IOException {

        if (!ah.parseOptions()) {
            return;
        }

        Ptx ptx = ah.getPtx();
        PrintWriter out = ah.getWriter();

        try {
            File rootDir = AppProperties.getContainerRootDir();
            FileDataContainer logContainer = new FileDataContainer(rootDir, ah.getProject(), ptx);
            BufferedInputStream bis = logContainer.getStream();
            GZIPInputStream gzis = new GZIPInputStream(bis);
            InputStreamReader isr = new InputStreamReader(gzis);

            char[] buf = new char[Http.BUFFER_SIZE];
            int len;
            while ((len = isr.read(buf, 0, buf.length)) != -1) {
                out.write(buf, 0, len);
            }
            bis.close();

        } catch (IOException e) {
            ah.errorMessage(ActionMessages.getString("GetLogFile.2") + ptx.getIdString()); //$NON-NLS-1$
        } catch (Exception e) {
            ah.errorMessage(e.getMessage());
        }
    }
}
