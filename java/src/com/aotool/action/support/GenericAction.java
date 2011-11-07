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

package com.aotool.action.support;

import java.io.FileNotFoundException;
import java.io.IOException;

import org.apache.log4j.Logger;

import com.aotool.web.servlet.Session;

/**
 * GenericAction is an abstract class from which all actions must inherit.
 */
public abstract class GenericAction {

    protected final static String BARS = "----------------------------"; //$NON-NLS-1$

    protected static final Logger logger = Logger.getLogger(Session.class);

    public abstract String getSummary();

    public abstract String getHelpMessage();

    public abstract void doAction(ActionHelper ah) throws FileNotFoundException, IOException;
}
