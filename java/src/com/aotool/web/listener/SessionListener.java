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

package com.aotool.web.listener;

import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionEvent;
import javax.servlet.http.HttpSessionListener;

import org.apache.log4j.Logger;

/**
 * Tracks session creation and expiration events. PTX closure must be handled
 * implicitly by this session listener as well as explicitly via a servlet. In
 * the event the client aborts for any reason (power hit, core dump), this
 * ensures the PTX will not be left open forever.
 */
public class SessionListener implements HttpSessionListener {

    private static final Logger logger = Logger.getLogger(SessionListener.class);

    public void sessionCreated(HttpSessionEvent event) {

        HttpSession session = event.getSession();

        if (logger.isInfoEnabled()) {
            logger.info("HTTP session started" //
                    + ", sessionId = " + session.getId()); //$NON-NLS-1$
        }
    }

    public void sessionDestroyed(HttpSessionEvent event) {

        HttpSession session = event.getSession();
        if (logger.isInfoEnabled()) {
            logger.info("HTTP session ended" //
                    + ", sessionId = " + session.getId()); //$NON-NLS-1$
        }
    }
}
