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
import java.io.PrintWriter;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import org.apache.log4j.Level;
import org.apache.log4j.Logger;

import com.aotool.web.AppProperties;
import com.aotool.web.Http;

/**
 * The Session servlet is used to begin an HTTP session. It sends the session ID
 * back to the client so that subsequent connections may participate in the same
 * session. A session must exist before a PTX may be started.
 */
public final class Session extends AOServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(Session.class);

    @Override
    public void doGet(HttpServletRequest req, HttpServletResponse res) throws ServletException,
            IOException {

        // Allow client to override default logging level (undocumented).
        String log_level = req.getParameter(Http.LOG_LEVEL_PARAM);
        if (log_level != null) {
            if ("on".equalsIgnoreCase(log_level)) { //$NON-NLS-1$
                Logger.getRootLogger().setLevel(Level.toLevel("ALL")); //$NON-NLS-1$
            } else {
                Logger.getRootLogger().setLevel(Level.toLevel(log_level));
            }
        }

        /*
         * Compare client version versus server and complain unless compatible.
         * Currently we allow the client to lag the server by one at the RHS,
         * e.g. server=1.0.8, client=1.0.7 is allowed.
         * We may need to be more sophisticated someday.
         */
        PrintWriter out = res.getWriter();
        String client_ver = req.getParameter(Http.CLIENT_VERSION_PARAM);
        String server_ver = AppProperties.getApplicationVersion();
        if (client_ver == null) {
            out.print(Http.warningMessage("unable to find client version"));
        } else if (server_ver == null) {
            out.print(Http.warningMessage("unable to find server version"));
        } else if (!client_ver.equals(server_ver)) {
            StringBuilder msg = new StringBuilder();
            msg.append(" client (");
            msg.append(client_ver);
            msg.append(") and server (");
            msg.append(server_ver);
            msg.append(")");
            
            String[] cv = client_ver.split("[.]");
            String[] sv = server_ver.split("[.]");
            if (cv.length == sv.length) {
                int len = cv.length - 1;
                for (int i = 0; i < len; i++) {
                    if (!cv[i].equals(sv[i])) {
                        throw new ServletException("incompatible" + msg.toString());
                    }
                }
                if (Integer.valueOf(cv[len]) + 1 != Integer.valueOf(sv[len])) {
                    throw new ServletException("incompatible" + msg.toString());
                } else {
                    out.print(Http.warningMessage("differing" + msg.toString()));
                }
            } else {
                throw new ServletException("incompatible" + msg.toString());
            }
        }

        // Start a new session.
        HttpSession session = req.getSession(true);

        /*
         * Allow the client to override the default session timeout which we
         * have set in web.xml
         */
        String timeout = req.getParameter(Http.SESSION_TIMEOUT_SECS_PARAM);
        if (timeout != null && Integer.parseInt(timeout) > 0) {
            session.setMaxInactiveInterval(Integer.parseInt(timeout));
        }

        // Report the ID of our new session back to the client.
        res.addHeader(Http.X_SET_PROPERTY_HEADER, Http.SESSION_ID_PROPERTY + '=' + session.getId());

        /*
         * Log the new session. This might be more elegant in the session
         * listener but then it wouldn't have access to the project name.
         */
        if (logger.isInfoEnabled()) {
            String projectName = req.getParameter(Http.PROJECT_NAME_PARAM);
            logger.info("***** SESSION STARTED" //
                    + ", sessionId = " + session.getId() //
                    + ", projectName = " + projectName);
        }
    }
}
