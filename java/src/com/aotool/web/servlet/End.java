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

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import org.apache.log4j.Logger;

import com.aotool.entity.DataService;
import com.aotool.entity.DataServiceHelper;
import com.aotool.entity.Ptx;
import com.aotool.entity.PtxBuilder;
import com.aotool.web.Http;
import com.aotool.web.SessionHelper;

/**
 * The End servlet is invoked to close out a PTX and end its session.
 */
public final class End extends AOServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(End.class);

    @Override
    public void doGet(HttpServletRequest req, HttpServletResponse res) throws ServletException {

        HttpSession session = req.getSession(false);
        SessionHelper sessionHelper = new SessionHelper(session);
        DataService dataService = null;

        try {
            dataService = DataServiceHelper.create(getServletContext());
            dataService.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(dataService, sessionHelper.getCurrentPtxId());
            Ptx ptx = ptxBuilder.getPtx();

            // determine exit status.
            String rcstr = req.getHeader(Http.X_CLIENT_STATUS_HEADER);
            ptx.setResult(Integer.parseInt(rcstr) == 0);

            String recycledstr = req.getHeader(Http.X_RECYCLED_COUNT_HEADER);
            ptx.setDownloadedCount(Integer.parseInt(recycledstr));

            dataService.commitTransaction();

            sessionHelper.removeCurrentPtxId(true);

        } catch (Exception e) {
            DataServiceHelper.rollbackTx(dataService);
            throw new ServletException(e);
        } finally {
            DataServiceHelper.close(dataService);
        }

        /*
         * Shut the session down. This will trigger the session listener to
         * complete the finalization process.
         */
        if (logger.isInfoEnabled()) {
            logger.info("Closing HTTP session" //
                    + ", sessionId = " + session.getId()); //$NON-NLS-1$
        }
        session.invalidate();
    }
}
