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

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.locks.ReentrantLock;

import javax.persistence.EntityManager;
import javax.persistence.LockModeType;
import javax.persistence.Query;

import org.apache.log4j.Logger;

import com.aotool.web.AppProperties;

/**
 * 
 */
public class DataService {

    private static final Logger logger = Logger.getLogger(DataService.class);

    private static final Map<Long, ReentrantLock> ptxLocks = new HashMap<Long, ReentrantLock>();
    private ThreadLocal<Long> lockedPtxId = new ThreadLocal<Long>();

    protected EntityManager em;

    public DataService(EntityManager em) {
        this.em = em;
    }

    public void close() {
        if (em != null) {
            em.close();
        }
    }

    /**
     * Begin a database transaction.
     */
    public void beginTransaction() {

        if (logger.isDebugEnabled()) {
            logger.debug("Beginning TX");
        }

        em.getTransaction().begin();

        if (logger.isDebugEnabled()) {
            logger.debug("Began TX");
        }
    }

    /**
     * Commit the current database transaction.
     */
    public void commitTransaction() {

        if (logger.isDebugEnabled()) {
            logger.debug("Committing TX");
        }

        em.getTransaction().commit();

        removeLocks();

        if (logger.isDebugEnabled()) {
            logger.debug("Committed TX");
        }
    }

    /**
     * Rollback the current database transaction.
     */
    public void rollbackTransaction() {

        if (logger.isDebugEnabled()) {
            logger.debug("Rolling back TX");
        }

        if (em.getTransaction().isActive()) {
            em.getTransaction().rollback();
        }

        removeLocks();

        if (logger.isDebugEnabled()) {
            logger.debug("Rolled back TX");
        }
    }

    private void removeLocks() {
        if (lockedPtxId.get() != null) {
            synchronized (ptxLocks) {
                Long ptxId = lockedPtxId.get();
                ReentrantLock locker = ptxLocks.get(ptxId);
                if (!locker.hasQueuedThreads()) {
                    ptxLocks.remove(ptxId);
                }
                locker.unlock();
            }
        }
    }

    public <T> T find(Class<T> clazz, Object key) {

        return em.find(clazz, key);
    }

    public <T> T findAndLock(Class<T> clazz, Object key) {

        if (AppProperties.needsSyntheticDatabaseLocks()) {
            if (Ptx.class.isAssignableFrom(clazz)) {
                Long ptxId = (Long) key;
                ReentrantLock locker = null;
                synchronized (ptxLocks) {
                    locker = ptxLocks.get(key);
                    if (locker == null) {
                        locker = new ReentrantLock();
                        ptxLocks.put(ptxId, locker);
                    }
                }
                // <-- here the lock registration (lockedPtxId) can be lost, see
                // removeLocks()
                locker.lock();
                if (lockedPtxId.get() != null) {
                    locker.unlock();
                    throw new IllegalStateException("Multiple locking is not supported");
                }
                synchronized (ptxLocks) {
                    // to make sure the lock is still registered
                    ptxLocks.put(ptxId, locker);
                }
                lockedPtxId.set(ptxId);
                return em.find(clazz, key);
            } else {
                throw new UnsupportedOperationException("Locking is not supported for class "
                        + clazz.getSimpleName());
            }
        } else {
            return em.find(clazz, key, LockModeType.PESSIMISTIC_WRITE);
        }
    }

    /**
     * Place a newly created object under database control.
     * 
     * @param object
     *            the object
     */
    public <T> void persist(T object) {
        if (logger.isTraceEnabled()) {
            logger.trace("Persisting" //
                    + ", class = " + object.getClass().getSimpleName() //
                    + ", value = " + object.toString());
        }

        em.persist(object);

        if (logger.isTraceEnabled()) {
            logger.trace("Persisted" //
                    + ", class = " + object.getClass().getSimpleName() //
                    + ", value = " + object.toString());
        }
    }

    /**
     * Propagate modifications made to a previously-persisted object.
     * 
     * @param object
     *            the object
     */
    public <T> T merge(T object) {

        if (logger.isTraceEnabled()) {
            logger.trace("Merging" //
                    + ", class = " + object.getClass().getSimpleName() //
                    + ", value = " + object.toString());
        }

        T result = em.merge(object);

        if (logger.isTraceEnabled()) {
            logger.trace("Merged" //
                    + ", class = " + object.getClass().getSimpleName() //
                    + ", value = " + object.toString());
        }
        return result;
    }

    public <T> void remove(T object) {

        if (object == null) {
            if (logger.isInfoEnabled()) {
                logger.info("Unable to remove null object");
            }
        } else {
            if (logger.isDebugEnabled()) {
                logger.debug("Removing" //
                        + ", class = " + object.getClass().getSimpleName() //
                        + ", value = " + object.toString());
            }

            em.remove(object);

            if (logger.isDebugEnabled()) {
                logger.debug("Removed" //
                        + ", class = " + object.getClass().getSimpleName() //
                        + ", value = " + object.toString());
            }
        }
    }

    public Query createQuery(String name) {

        return em.createQuery(name);
    }

    /**
     * Gets the project list.
     * 
     * @return the project list
     */
    public List<Project> getProjectList() {

        Query query = em.createNamedQuery("listEveryProject"); //$NON-NLS-1$
        @SuppressWarnings("unchecked")
        List<Project> projects = query.getResultList();
        return projects;
    }
}
