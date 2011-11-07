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

import java.io.IOException;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import org.apache.log4j.Logger;

import com.aotool.entity.DataService;
import com.aotool.entity.DataServiceHelper;
import com.aotool.entity.Moment;
import com.aotool.entity.Ptx;
import com.aotool.entity.PtxBuilder;
import com.aotool.web.Http;
import com.aotool.web.SessionHelper;

/**
 * The Start servlet is invoked to initialize a PTX and get it ready to receive
 * audit packets from the client. The PTX is active from this moment until it is
 * marked "done" during end processing.
 */
public final class Start extends AOServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(Start.class);

    @Override
    public void doGet(HttpServletRequest request, HttpServletResponse response)
            throws ServletException, IOException {

        HttpSession session = request.getSession(false);
        if (session == null) {
            throw new ServletException("missing session");
        }
        SessionHelper sessionHelper = new SessionHelper(session);
        DataService dataService = null;

        try {
            dataService = DataServiceHelper.create(getServletContext());
            dataService.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(dataService);
            Ptx ptx = ptxBuilder.getPtx();

            // Record the other build-specific parameters.
            ptx.setBaseDirectory(request.getParameter(Http.BASE_DIR_PARAM));
            ptx.setRelativeWorkingDirectory(request.getParameter(Http.RWD_PARAM));
            ptx.setLogName(request.getParameter(Http.LOGIN_NAME_PARAM));
            ptx.setGroupName(request.getParameter(Http.GROUP_NAME_PARAM));
            ptx.setHostName(request.getParameter(Http.HOST_NAME_PARAM));
            ptx.setSystemName(request.getParameter(Http.SYSTEM_NAME_PARAM));
            ptx.setOsRelease(request.getParameter(Http.OS_RELEASE_PARAM));
            ptx.setMachineType(request.getParameter(Http.MACHINE_TYPE_PARAM));
            ptx.setClientStartTime(new Moment(request.getParameter(Http.CLIENT_START_TIME_PARAM)));
            ptx.setReadOnly(request.getParameter(Http.READ_ONLY_PARAM) == null);

            String projectName = request.getParameter(Http.PROJECT_NAME_PARAM);
            ptxBuilder.assignPtxToProject(projectName);

            sessionHelper.setCurrentPtxId(ptx.getId());

            if (logger.isDebugEnabled()) {
                logger.debug("New Ptx" //
                        + ", ptxId = " + ptx.getIdString() //
                        + ", sessionId = " + session.getId());
            }

            dataService.commitTransaction();

        } catch (Exception e) {
            DataServiceHelper.rollbackTx(dataService);
            throw new ServletException(e);
        } finally {
            DataServiceHelper.close(dataService);
        }
    }
}
