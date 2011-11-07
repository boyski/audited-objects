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

package com.aotool.web.servlet;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import org.apache.log4j.Logger;

import com.aotool.Messages;
import com.aotool.entity.DataService;
import com.aotool.entity.DataServiceHelper;
import com.aotool.entity.PathState;
import com.aotool.entity.Project;
import com.aotool.entity.Ptx;
import com.aotool.entity.PtxBuilder;
import com.aotool.util.FileDataContainer;
import com.aotool.web.AppProperties;
import com.aotool.web.Http;
import com.aotool.web.SessionHelper;

/**
 * The Upload servlet accepts an uploaded file from the client and stores a
 * compressed copy on the server side unless an identical copy already exists.
 * <B>This servlet does not change any database state; the only persistent
 * change it makes is the potential presence of a new file container.</B>
 */
public final class Upload extends AOServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(Upload.class);

    @Override
    public void doPost(HttpServletRequest req, HttpServletResponse res) throws IOException,
            ServletException {

        HttpSession session = req.getSession(false);
        SessionHelper sessionHelper = new SessionHelper(session);
        DataService dataService = null;
        PrintWriter out = res.getWriter();

        /*
         * This information must be sent as headers, not parameters, because
         * reading parameters causes the input stream to be read and discarded.
         * An alternative would be to use a form-based upload, which would allow
         * parameters.
         */
        boolean gzipped = req.getHeader(Http.X_GZIPPED_HEADER) != null;
        boolean logfile = req.getHeader(Http.X_LOGFILE_HEADER) != null;

        String csv = req.getHeader(Http.X_PATHSTATE_HEADER);
        PathState ps = new PathState.Builder(csv).build();

        try {
            String verb;
            FileDataContainer container;
            File rootDir = AppProperties.getContainerRootDir();

            dataService = DataServiceHelper.create(getServletContext());
            dataService.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(dataService, sessionHelper.getCurrentPtxId(),
                    true);
            Ptx ptx = ptxBuilder.getPtx();
            Project project = ptx.getProject();

            if (logfile) {
                container = new FileDataContainer(rootDir, project, ptx);
            } else {
                container = new FileDataContainer(rootDir, project, ps);
                ptx.bumpUploadedCount();
            }

            dataService.commitTransaction();

            if (!container.exists()) {
                BufferedInputStream bin = new BufferedInputStream(req.getInputStream());
                container.storeStream(bin, gzipped);
                out.print(Http.noteMessage(Messages.getString("Upload.0"), ps.getPathString()));
                verb = "done";
            } else {
                verb = "skipped";
            }

            if (logger.isInfoEnabled()) {
                String msg = "Archiving " + verb + ", container = " + container.toStandardString();
                logger.info(msg);
            }
        } catch (Exception e) {
            DataServiceHelper.rollbackTx(dataService);
            throw new ServletException(e);
        } finally {
            DataServiceHelper.close(dataService);
        }
    }
}
