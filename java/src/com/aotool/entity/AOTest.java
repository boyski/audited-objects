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

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;
import javax.persistence.Persistence;

import org.junit.After;
import org.junit.Before;

public abstract class AOTest {

    protected EntityManagerFactory emf;
    protected TestDataService ds;

    @Before
    public void begin() {
        emf = Persistence.createEntityManagerFactory(DataServiceHelper.PERSISTENCE_UNIT_NAME);
        ds = new TestDataService(emf);
    }

    @After
    public void end() {
        ds.close();
        emf.close();
    }

    public static class TestDataService extends DataService {

        public TestDataService(EntityManagerFactory emf) {
            super(emf.createEntityManager());
        }

        public EntityManager getEntityManager() {
            return em;
        }
    }
}
