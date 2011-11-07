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

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import org.apache.log4j.Logger;

import com.aotool.action.support.ActionException;
import com.aotool.action.support.Options;
import com.aotool.entity.Project;
import com.aotool.web.AppProperties;
import com.aotool.web.Http;

/**
 * The Ping servlet checks whether the server is alive and returns the address
 * at which it was found. The client may invoke this once in a while in order to
 * keep the HTTP session alive.
 */
public final class Ping extends AOServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(Ping.class);

    @Override
    public void doGet(HttpServletRequest req, HttpServletResponse res) throws IOException {

        res.setContentType(Http.TEXT_PLAIN);
        PrintWriter out = res.getWriter();
        
        /*
         * It's important to always retrieve the session here because
         * the ping servlet is sometimes used as a session keep-alive.
         */
        HttpSession session = req.getSession(false);


        String projectName = req.getParameter(Http.PROJECT_NAME_PARAM);
        Project project = new Project(projectName);
        Options opts = new Options("ping", project, req.getParameterMap());
        opts.registerLongFlag();
        try {
            String message = opts.parse();
            if (message != null) {
                out.print(Http.noteMessage(message));
            }
        } catch (ActionException e) {
        }

        if (logger.isInfoEnabled()) {
            StringBuilder msg = new StringBuilder("PING"); //$NON-NLS-1$

            String remote = req.getRemoteHost();
            if (remote != null) {
                msg.append(" from ").append(remote); //$NON-NLS-1$
            }

            if (session != null) {
                msg.append(" in session ").append(session.getId()); //$NON-NLS-1$
            }

            logger.info(msg.toString());
        }
        
        String server_ver = AppProperties.getApplicationVersion();
        String client_ver = req.getParameter(Http.CLIENT_VERSION_PARAM);
        if (!server_ver.equals(client_ver)) {
            StringBuilder sb = new StringBuilder();
            sb.append("incompatible client (");
            sb.append(client_ver);
            sb.append(") and server (");
            sb.append(server_ver);
            sb.append(")");
            out.print(Http.errorMessage(sb.toString()));
            return;
        }

        String server = req.getServerName();
        String port = new Integer(req.getServerPort()).toString();
        out.print(server + ':' + port);

        if (opts.hasLongFlag()) {
            out.print(" (client=" + client_ver + ", server=" + server_ver + ")");
        }

        out.println("");
    }
}
