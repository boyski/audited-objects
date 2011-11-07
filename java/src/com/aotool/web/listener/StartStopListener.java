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

import java.io.IOException;

import javax.servlet.ServletContext;
import javax.servlet.ServletContextEvent;
import javax.servlet.ServletContextListener;

import org.apache.log4j.Logger;

import com.aotool.entity.DataServiceHelper;
import com.aotool.web.AppProperties;

/**
 * The listener interface for receiving server start and stop events.
 * Responsible for application initialization and finalization handling.
 */
public final class StartStopListener implements ServletContextListener {

    private static final Logger logger = Logger.getLogger(StartStopListener.class);

    public void contextInitialized(ServletContextEvent event) {

        ServletContext context = event.getServletContext();
        try {
            AppProperties.initialize(context);
            DataServiceHelper.initialize(context);
        } catch (IOException e) {
            logger.error("", e);
        }
    }

    public void contextDestroyed(ServletContextEvent event) {

        DataServiceHelper.destroy(event.getServletContext());
    }
}
