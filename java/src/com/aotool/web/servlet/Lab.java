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

import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.log4j.Logger;

import com.aotool.web.Http;

/**
 * The Lab servlet is not used by the real application; it exists as a
 * laboratory for development prototyping. At any given time its contents are
 * likely to be the last thing played with, and can generally be thrown away.
 */
public final class Lab extends HttpServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(Lab.class);

    @Override
    public void doGet(HttpServletRequest req, HttpServletResponse res) throws IOException {
        res.setContentType(Http.TEXT_PLAIN);
        PrintWriter out = res.getWriter();
        String msg = "LAB GET";
        String param = req.getParameter("param");
        msg += " param=" + (param == null ? "(null)" : param);
        logger.info(msg);
        out.println(msg);
    }

    @Override
    public void doPost(HttpServletRequest req, HttpServletResponse res) throws IOException {
        res.setContentType(Http.TEXT_PLAIN);
        PrintWriter out = res.getWriter();
        String msg = "LAB POST";
        String param = req.getParameter("param");
        msg += " param=" + (param == null ? "(null)" : param);
        logger.info(msg);
        out.println(msg);
    }
}
