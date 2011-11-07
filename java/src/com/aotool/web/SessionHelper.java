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

package com.aotool.web;

import javax.persistence.EntityManagerFactory;
import javax.servlet.ServletException;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionBindingEvent;
import javax.servlet.http.HttpSessionBindingListener;

import org.apache.log4j.Logger;

import com.aotool.Messages;
import com.aotool.entity.DataService;
import com.aotool.entity.DataServiceHelper;
import com.aotool.entity.PtxBuilder;

public class SessionHelper {

    private interface SessionAttrNames {
        String CURRENT_PTX_ID = "CURRENT_PTX_ID"; //$NON-NLS-1$
    }

    private HttpSession session;

    public SessionHelper(HttpSession session) {
        this.session = session;
    }

    /**
     * Sets the current PTX.
     * 
     * @param ptxId
     *            the id of the PTX to be made current
     */
    public void setCurrentPtxId(Long ptxId) {

        session.setAttribute(SessionAttrNames.CURRENT_PTX_ID, new PtxIdHolder(ptxId));
    }

    /**
     * Gets the current PTX object.
     * 
     * @return the current PTX object
     * 
     * @throws ServletException
     *             a general-purpose exception
     */
    public Long getCurrentPtxId() throws ServletException {
        PtxIdHolder holder = (PtxIdHolder) session.getAttribute(SessionAttrNames.CURRENT_PTX_ID);
        if (holder == null) {
            throw new ServletException(Messages.getString("Constants.0")); //$NON-NLS-1$
        }
        Long ptxId = holder.getPtxId();
        if (ptxId == null) {
            throw new ServletException(Messages.getString("Constants.0")); //$NON-NLS-1$
        }
        return ptxId;
    }

    public void removeCurrentPtxId(boolean ptxIsCompleted) {

        PtxIdHolder ptxIdHolder = (PtxIdHolder) session
                .getAttribute(SessionAttrNames.CURRENT_PTX_ID);
        ptxIdHolder.isCompleted = ptxIsCompleted;
        session.removeAttribute(SessionAttrNames.CURRENT_PTX_ID);
    }

    private static class PtxIdHolder implements HttpSessionBindingListener {

        private static final Logger logger = Logger.getLogger(SessionHelper.class);

        public Long ptxId;
        public boolean isCompleted = false;

        public PtxIdHolder(Long ptxId) {
            this.ptxId = ptxId;
        }

        public Long getPtxId() {
            return ptxId;
        }

        public void valueBound(HttpSessionBindingEvent paramHttpSessionBindingEvent) {
        }

        public void valueUnbound(HttpSessionBindingEvent event) {

            if (!isCompleted) {
                if (logger.isDebugEnabled()) {
                    logger.debug("Marking timed-out Ptx as failed" + ", ptxId = " + ptxId);
                }
                EntityManagerFactory emf = DataServiceHelper.createEntityManagerFactory();
                DataService dataService = null;

                try {
                    dataService = DataServiceHelper.create(emf);
                    dataService.beginTransaction();

                    PtxBuilder ptxBuilder = new PtxBuilder(dataService, ptxId);
                    ptxBuilder.setResult(false);

                    dataService.commitTransaction();

                } catch (Exception e) {
                    logger.error("", e);
                    DataServiceHelper.rollbackTx(dataService);
                } finally {
                    DataServiceHelper.close(dataService);
                }

                if (logger.isDebugEnabled()) {
                    logger.debug("Marked timed-out Ptx as failed" + ", ptxId = " + ptxId);
                }
            }
        }
    }
}
