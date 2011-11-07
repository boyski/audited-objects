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

package com.aotool.entity;

import java.util.Map;

import javax.persistence.EntityManagerFactory;
import javax.persistence.Persistence;
import javax.servlet.ServletContext;

import org.apache.log4j.Logger;

import com.aotool.web.AppProperties;

public class DataServiceHelper {

    public static final String PERSISTENCE_UNIT_NAME = "AODB"; //$NON-NLS-1$

    public static final String SERVLETCTX_EMF = "emf"; //$NON-NLS-1$

    private static final Logger logger = Logger.getLogger(DataServiceHelper.class);

    private DataServiceHelper() {
    }

    public static void initialize(ServletContext context) {
        if (logger.isDebugEnabled()) {
            logger.debug("Creating an EntityManagerFactory");
        }
        EntityManagerFactory emf = createEntityManagerFactory();
        context.setAttribute(SERVLETCTX_EMF, emf);
    }

    public static void destroy(ServletContext context) {
        if (logger.isDebugEnabled()) {
            logger.debug("Closing a EntityManagerFactory");
        }
        EntityManagerFactory emf = (EntityManagerFactory) context.getAttribute(SERVLETCTX_EMF);
        if (emf != null) {
            emf.close();
        }
    }

    public static EntityManagerFactory createEntityManagerFactory() {
        Map<String, String> jpaProps = AppProperties.getJpaProps();
        return Persistence.createEntityManagerFactory(PERSISTENCE_UNIT_NAME, jpaProps);
    }

    public static DataService create(ServletContext context) {
        EntityManagerFactory emf = (EntityManagerFactory) context.getAttribute(SERVLETCTX_EMF);
        return create(emf);
    }

    public static DataService create(EntityManagerFactory emf) {
        if (logger.isDebugEnabled()) {
            logger.debug("Creating a DataService");
        }
        return new DataService(emf.createEntityManager());
    }

    public static void close(DataService ds) {
        if (ds != null) {
            ds.close();
            if (logger.isDebugEnabled()) {
                logger.debug("Closing a DataService");
            }
        }
    }

    public static void rollbackTx(DataService ds) {
        if (ds != null) {
            ds.rollbackTransaction();
        }
    }
}
