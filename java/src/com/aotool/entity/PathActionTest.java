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
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import com.aotool.util.PathFormat;

public class PathActionTest extends AOTest {

    private static final String CA_CSV = "4526,0,4525,l0ruel.365yan,10,hostname,";
    private static final String CA_CSV2 = "4526,0,4525,20ruel.365yan,10,hostname,";
    private static final String PA_BUF = "R,,,,,,,,";
    private static final String PN_NAME = "/dir/filename.ext";
    private static final String PN_NAME2 = "/dir2/filename2.ext";
    private static final String TARGET = "C:/temp/dummyLink.h";
    private static final String PS_BUF1 = "f,?,l0ruel.16mv8g,8,plw,1sgxm0v";
    private static final String PS_BUF2 = "f,?,n0ruel.16mv8g,8,plw,1sgxm0v";
    private static final String PS_BUF3 = "f,?,q0ruel.16mv8g,8,plw,1sgxm0v";

    private String pathState(String psbuf, String target, String pathName) {
        return psbuf + "," + (target != null ? target : "") + "," + pathName;
    }

    private CommandAction createCommandAction(Ptx ptx, String caValue) {
        Command cmd = new Command();
        CommandAction ca = new CommandAction.Builder(ptx, cmd, caValue).build();
        return ca;
    }

    private PathAction addPathAction(CommandAction ca, String paValue, String psValue) {
        PathState ps = new PathState.Builder(psValue).build();
        PathAction pa = new PathAction.Builder(ps, paValue).build();
        ca.addPathAction(pa);
        return pa;
    }

    @Test
    public void createPathStateWithTarget() throws Exception {

        Long paId = null;
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);
            CommandAction ca = createCommandAction(ptxBuilder.getPtx(), CA_CSV);
            PathAction pa = addPathAction(ca, PA_BUF, pathState(PS_BUF1, TARGET, PN_NAME));
            ptxBuilder.setCommandAction(ca);

            paId = pa.getId();

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(paId);

            ds.beginTransaction();

            PathAction pa = ds.find(PathAction.class, paId);
            PathState ps = pa.getPathState();

            assertNotNull(ps);
            assertEquals("f", ps.getDataType());
            assertEquals("?", ps.getFsName());
            assertEquals("l0ruel.16mv8g", ps.getTimeStamp().toStandardString());
            assertEquals(8, ps.getSize());
            assertEquals(33188, ps.getMode());
            assertEquals("1sgxm0v", ps.getDataCode());
            assertEquals(TARGET, ps.getLinkTarget());

            assertNotNull(ps.getPathName());
            assertEquals(PN_NAME, ps.getPathName().getPathString());

            ds.commitTransaction();
        }
    }

    @Test
    public void createPathStateWithoutTarget() throws Exception {

        Long paId = null;
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);
            CommandAction ca = createCommandAction(ptxBuilder.getPtx(), CA_CSV);
            PathAction pa = addPathAction(ca, PA_BUF, pathState(PS_BUF1, null, PN_NAME));
            ptxBuilder.setCommandAction(ca);

            paId = pa.getId();

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(paId);

            ds.beginTransaction();

            PathAction pa = ds.find(PathAction.class, paId);
            PathState ps = pa.getPathState();

            assertNotNull(ps);
            assertEquals("f", ps.getDataType());
            assertEquals("?", ps.getFsName());
            assertEquals("l0ruel.16mv8g", ps.getTimeStamp().toStandardString());
            assertEquals(8, ps.getSize());
            assertEquals(33188, ps.getMode());
            assertEquals("1sgxm0v", ps.getDataCode());
            assertEquals("", ps.getLinkTarget());

            assertNotNull(ps.getPathName());
            assertEquals(PN_NAME, ps.getPathName().getPathString());

            ds.commitTransaction();
        }
    }

    @Test
    public void validateMultiplePathActionsForActionCommand() throws Exception {

        Long caId = null;
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);
            CommandAction ca = createCommandAction(ptxBuilder.getPtx(), CA_CSV);
            addPathAction(ca, PA_BUF, pathState(PS_BUF1, TARGET, PN_NAME));
            addPathAction(ca, PA_BUF, pathState(PS_BUF1, null, PN_NAME));
            ptxBuilder.setCommandAction(ca);

            caId = ca.getId();

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(caId);

            ds.beginTransaction();

            CommandAction ca = ds.find(CommandAction.class, caId);

            // there should be two PathStates
            assertTrue(ca.getPathActions().size() == 2);

            // check that PathStates are different
            boolean isNoTargetPathStateFound = false;
            boolean isWithTargetPathStateFound = false;
            for (PathAction pa : ca.getPathActions()) {
                String target = pa.getPathState().getLinkTarget();
                if (target != null) {
                    isNoTargetPathStateFound |= target.length() == 0;
                    isWithTargetPathStateFound |= target.length() > 0;
                }
            }
            assertTrue(isNoTargetPathStateFound);
            assertTrue(isWithTargetPathStateFound);

            ds.commitTransaction();
        }
    }

    @Test
    public void findByXns() throws Exception {

        Ptx ptx = null;
        Long paIdMax = null;
        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);
            ptx = ptxBuilder.getPtx();
            {
                CommandAction ca = createCommandAction(ptx, CA_CSV);
                addPathAction(ca, PA_BUF, pathState(PS_BUF1, null, PN_NAME));
                addPathAction(ca, PA_BUF, pathState(PS_BUF1, null, PN_NAME2));
                PathAction pa = addPathAction(ca, PA_BUF, pathState(PS_BUF3, null, PN_NAME));
                ptxBuilder.setCommandAction(ca);

                paIdMax = pa.getId();
            }
            {
                CommandAction ca = createCommandAction(ptx, CA_CSV2);
                addPathAction(ca, PA_BUF, pathState(PS_BUF2, null, PN_NAME));
                addPathAction(ca, PA_BUF, pathState(PS_BUF2, null, PN_NAME2));
                ptxBuilder.setCommandAction(ca);
            }

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(ptx);
            assertNotNull(ptx.getId());

            ds.beginTransaction();

            String fmt = PathFormat.format(ptx, PN_NAME);
            PathAction pa = PathFormat.findByExtendedName(fmt, ds);
            assertEquals(paIdMax, pa.getId());

            ds.commitTransaction();
        }
    }
}
