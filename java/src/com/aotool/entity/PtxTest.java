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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;

import org.apache.log4j.Logger;
import org.junit.Test;

public class PtxTest extends AOTest {

    private static final Logger logger = Logger.getLogger(PtxTest.class);

    private static final String CA_CSV = "4526,0,4525,l0ruel.365yan,10,hostname,";
    private static final String PA_BUF = "R,,,,,,,,";

    private static final String PN_NAME = "/dir/filename.ext";
    private static final String TARGET = "C:/temp/dummyLink.h";
    private static final String PS_BUF = "f,?,l0ruel.16mv8g,8,plw,1sgxm0v";
    private static final String PS_BUF_WITHTARGET = PS_BUF + "," + TARGET + "," + PN_NAME;

    @Test
    public void createPtx() throws Exception {

        Long ptxId = null;
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);
            ptxId = ptxBuilder.getPtx().getId();

            ptxBuilder.getPtx().setBaseDirectory("/vobs_ao/demo/hello");
            ptxBuilder.getPtx().setLogName("dsb");
            ptxBuilder.getPtx().setGroupName("ccusers");
            ptxBuilder.getPtx().setHostName("u20");
            ptxBuilder.getPtx().setSystemName("SunOS");
            ptxBuilder.getPtx().setOsRelease("5.10");
            ptxBuilder.getPtx().setMachineType("i86pc");
            ptxBuilder.getPtx().setClientStartTime(new Moment("khvcl9.270ggo"));

            ptxBuilder.assignPtxToProject("HelloWorld");

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(ptxId);

            ds.beginTransaction();

            Ptx ptx = ds.find(Ptx.class, ptxId);
            assertNotNull(ptx);
            assertNotNull(ptx.getIdString());

            ds.commitTransaction();
        }
    }

    @Test
    public void createPtxWithLabel() throws Exception {

        Long ptxId = null;
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);
            ptxId = ptxBuilder.getPtx().getId();

            ptxBuilder.label("label1");

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(ptxId);

            ds.beginTransaction();

            Ptx ptx = ds.find(Ptx.class, ptxId);
            assertNotNull(ptx);
            assertNotNull(ptx.getIdString());
            assertEquals(ptx.getLabel(), "label1");

            ds.commitTransaction();
        }
    }

    @Test
    public void getMostRecentPtx() throws Exception {

        String projectName = "HelloWorld";
        Long ptxId1 = null;
        Long ptxId2 = null;

        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder1 = new PtxBuilder(ds);
            ptxBuilder1.assignPtxToProject(projectName);
            ptxId1 = ptxBuilder1.getPtx().getId();

            PtxBuilder ptxBuilder2 = new PtxBuilder(ds);
            ptxBuilder2.assignPtxToProject(projectName);
            ptxId2 = ptxBuilder2.getPtx().getId();

            System.out.println(ptxId1 + ", " + ptxBuilder1.getPtx().getStartTime());
            System.out.println(ptxId2 + ", " + ptxBuilder2.getPtx().getStartTime());

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(ptxId1);
            assertNotNull(ptxId2);
            assertTrue(ptxId1 != ptxId2);

            ds.beginTransaction();

            Project project = ds.find(Project.class, projectName);
            assertNotNull(project);

            Ptx ptx = project.getMostRecentPtx();
            assertEquals(ptxId2, ptx.getId());

            ds.commitTransaction();
        }
    }

    @Test
    public void deletePtx() throws Exception {

        String projectName = "deletePtx1";
        Long ptxId = null;
        Long commandActionId = null;
        Long pathActionId = null;

        // create a project
        ds.getEntityManager().clear();
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);
            ptxBuilder.assignPtxToProject(projectName);
            Command cmd = new Command();
            CommandAction ca = new CommandAction.Builder(ptxBuilder.getPtx(), cmd, CA_CSV).build();
            PathState ps = new PathState.Builder(PS_BUF_WITHTARGET).build();
            PathAction pa = new PathAction.Builder(ps, PA_BUF).build();
            ca.addPathAction(pa);
            ptxBuilder.setCommandAction(ca);

            ptxId = ptxBuilder.getPtx().getId();
            commandActionId = ca.getId();
            pathActionId = pa.getId();

            ds.commitTransaction();
        }

        // delete a Ptx
        ds.getEntityManager().clear();
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds, ptxId);
            ptxBuilder.deletePtx();

            ds.commitTransaction();
        }

        // test
        ds.getEntityManager().clear();
        {
            assertNotNull(ptxId);
            assertNotNull(commandActionId);
            assertNotNull(pathActionId);

            ds.beginTransaction();

            Project project = ds.find(Project.class, projectName);
            assertNotNull(project);

            Ptx ptx = ds.find(Ptx.class, ptxId);
            assertNull(ptx);

            CommandAction ca = ds.find(CommandAction.class, commandActionId);
            assertNull(ca);

            PathAction pa = ds.find(PathAction.class, pathActionId);
            assertNull(pa);

            ds.commitTransaction();
        }
    }

    @Test
    public void deleteProject() throws Exception {

        String projectName = "deleteProject1";
        Long ptxId = null;
        Long commandActionId = null;
        Long pathActionId = null;

        // create a project
        ds.getEntityManager().clear();
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);
            ptxBuilder.assignPtxToProject(projectName);
            Command cmd = new Command();
            CommandAction ca = new CommandAction.Builder(ptxBuilder.getPtx(), cmd, CA_CSV).build();
            PathState ps = new PathState.Builder(PS_BUF_WITHTARGET).build();
            PathAction pa = new PathAction.Builder(ps, PA_BUF).build();
            ca.addPathAction(pa);
            ptxBuilder.setCommandAction(ca);

            ptxId = ptxBuilder.getPtx().getId();
            commandActionId = ca.getId();
            pathActionId = pa.getId();

            ds.commitTransaction();
        }

        // delete a project
        ds.getEntityManager().clear();
        {
            ds.beginTransaction();

            Project project = ds.find(Project.class, projectName);
            ds.remove(project);

            ds.commitTransaction();
        }

        // test
        ds.getEntityManager().clear();
        {
            assertNotNull(ptxId);
            assertNotNull(commandActionId);
            assertNotNull(pathActionId);

            ds.beginTransaction();

            Project project = ds.find(Project.class, projectName);
            assertNull(project);

            Ptx ptx = ds.find(Ptx.class, ptxId);
            assertNull(ptx);

            CommandAction ca = ds.find(CommandAction.class, commandActionId);
            assertNull(ca);

            PathAction pa = ds.find(PathAction.class, pathActionId);
            assertNull(pa);

            ds.commitTransaction();
        }
    }

    static class BumpUploadedCount implements Runnable {

        EntityManagerFactory emf;
        long ptxId;
        long sleep;

        BumpUploadedCount(EntityManagerFactory emf, long ptxId, long sleep) {
            this.emf = emf;
            this.ptxId = ptxId;
            this.sleep = sleep;
        }

        public void run() {

            logger.debug("BumpUploadedCount begin");

            EntityManager em = emf.createEntityManager();
            DataService ds = new DataService(em);

            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds, ptxId, true);
            sleep(sleep);
            logger.debug("ptx.uploadedCount = " + ptxBuilder.getPtx().getUploadedCount());
            ptxBuilder.getPtx().bumpUploadedCount();

            ds.commitTransaction();

            ds.close();

            logger.debug("BumpUploadedCount end");
        }
    }

    @Test
    public void doConcurrentPtxUpdates() throws Exception {

        Long ptxId = null;
        int updateCount = 5;
        int updateStartSleep = 100;
        int updateInSleep = 140;

        // create a Ptx
        ds.getEntityManager().clear();
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);
            ptxId = ptxBuilder.getPtx().getId();

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(ptxId);

            ExecutorService executorService = Executors.newFixedThreadPool(updateCount);

            for (int i = 0; i < updateCount; i++) {
                sleep(updateStartSleep);
                executorService.execute(new BumpUploadedCount(emf, ptxId, updateInSleep));
            }

            executorService.shutdown();
            executorService.awaitTermination((updateStartSleep + updateInSleep) * updateCount * 2,
                    TimeUnit.MILLISECONDS);
        }

    }

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }
}
