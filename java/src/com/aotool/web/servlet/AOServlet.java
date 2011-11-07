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
import java.util.Map;

import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.log4j.Logger;

import com.aotool.web.Http;

/**
 * The AOServlet class provides common facilities, such as logging and error
 * handling, for the convenience of the servlets in this package. Therefore,
 * servlets in this package should generally extend AOServlet rather than
 * HttpServlet.
 */
public class AOServlet extends HttpServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(AOServlet.class);

    private static long totalWorkTime = 0;
    private static long inProcessBusyTime = 0;
    private static long firstCallStartTime = 0;
    private static long callsInProcess = 0;
    private static final Object timeLoggingLock = new Object();

    @Override
    public void service(HttpServletRequest request, HttpServletResponse response)
            throws IOException {

        long startTime = 0;
        if (logger.isDebugEnabled()) {
            startTime = loggingStart(request.getParameterMap());
        }

        // Set the default content type.
        response.setContentType(Http.TEXT_PLAIN);

        // Print a generic message for all exceptions.
        try {
            super.service(request, response);

        } catch (Exception e) {

            logger.error("Http-call - exception" //
                    + ", servlet = " + this.getClass().getSimpleName(), e);

            if (response.isCommitted()) {
                /*
                 * We prefer to handle problems with an explicit error message
                 * to the client. But if it's too late for that, throw a
                 * server-side exception in preference to ignoring the error.
                 */
                throw new IOException(e.getMessage());
            } else {
                StringBuilder sb = new StringBuilder('2').append(' ');
                String msg = e.getMessage();
                if (msg != null) {
                    sb.append(msg);
                } else {
                    sb.append(e.toString());
                }
                response.setHeader(Http.X_SERVER_STATUS_HEADER, sb.toString());
            }
        }

        if (logger.isDebugEnabled()) {
            loggingEnd(startTime);
        }
    }

    private long loggingStart(Map<String, String[]> requestParams) {
        logger.debug("Http-call start" //
                + ", servlet = " + this.getClass().getSimpleName() //
                + ", request = " + convertMapToString(requestParams));
        long startTime = System.currentTimeMillis();
        synchronized (timeLoggingLock) {
            if (callsInProcess == 0) {
                firstCallStartTime = startTime;
            }
            callsInProcess++;
        }
        return startTime;
    }

    private void loggingEnd(long startTime) {
        long endTime = System.currentTimeMillis();
        long workTime = endTime - startTime;
        boolean inProcessBusyTimeUpdated = false;
        synchronized (timeLoggingLock) {
            callsInProcess--;
            if (callsInProcess == 0) {
                inProcessBusyTime += endTime - firstCallStartTime;
                inProcessBusyTimeUpdated = true;
            }
            totalWorkTime += workTime;
        }
        logger.debug("Http-call end" //
                + ", servlet = " + this.getClass().getSimpleName() //
                + ", workTime = " + workTime //
                + ", totalWorkTime = " + totalWorkTime //
                + ", inProcessBusyTime = " + inProcessBusyTime //
                + (inProcessBusyTimeUpdated ? " (updated)" : "") //
        );
    }

    private String convertMapToString(Map<String, String[]> map) {
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String[]> e : map.entrySet()) {
            if (sb.length() > 0) {
                sb.append(',');
            }
            sb.append(e.getKey());
            sb.append("=['");
            boolean first = true;
            for (String value : e.getValue()) {
                if (!first) {
                    sb.append("','");
                    first = false;
                }
                sb.append(value);
            }
            sb.append("']");
        }
        return sb.toString();
    }
}
