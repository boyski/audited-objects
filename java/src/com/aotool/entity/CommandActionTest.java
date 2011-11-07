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

import com.aotool.Constants;

public class CommandActionTest extends AOTest {

    private static final String CA_CSV = "4526,0,4525,l0ruel.365yan,10,hostname,";

    private static final String CMD_CSV = "make,src,1i2n739+27,1niw8in+11,xrzrgy-70,make all-am";

    private static final String BUF1 = "R,open,0.0,9065,1,9064,0,aford5+14,aford5+14,fgk1f1+129,f,?,iaou0o.0,1755,plw,,/usr/include/iso/stdarg_c99.h";
    private static final String BUF2 = "R,open,0.0,9065,1,9064,0,aford5+14,aford5+14,fgk1f1+129,f,?,iaou0o.0,1847,plw,,/usr/include/iso/stdarg_iso.h";
    private static final String BUF3 = "R,open,0.0,9065,1,9064,0,aford5+14,aford5+14,fgk1f1+129,f,?,iaou0o.0,2586,plw,,/usr/include/iso/stdio_c99.h";
    private static final String BUF4 = "R,open,0.0,9065,1,9064,0,aford5+14,aford5+14,fgk1f1+129,f,?,iaou0o.0,1847,plw,,/usr/include/iso/stdarg_iso.h";

    @Test
    public void createCommandAction() {

        Long caId = null;

        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);

            Command cmd = new Command.Builder(CMD_CSV).build();
            cmd.setAggregated();

            CommandAction ca = new CommandAction.Builder(ptxBuilder.getPtx(), cmd, CA_CSV).build();
            ptxBuilder.setCommandAction(ca);
            caId = ca.getId();

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(caId);

            ds.beginTransaction();

            CommandAction ca = ds.find(CommandAction.class, caId);

            assertEquals(Long.parseLong("l0ruel", Constants.CSV_RADIX), ca.getStartTime()
                    .getSeconds());
            assertEquals(Long.parseLong("365yan", Constants.CSV_RADIX), ca.getStartTime()
                    .getNanoseconds());
            assertEquals(null, ca.getHostName());

            Command cmd = ca.getCommand();
            assertEquals("make", cmd.getProgram());
            assertEquals("src", cmd.getRelativeWorkingDirectory());
            assertEquals("1i2n739+27", cmd.getPccode());
            assertEquals("1niw8in+11", cmd.getCcode());
            assertEquals("xrzrgy-70", cmd.getPathCode());
            assertEquals("make all-am", cmd.getLine());
            assertTrue(cmd.isAggregated());

            ds.commitTransaction();
        }
    }

    @Test
    public void createCommandActionWithRepeatPathName() {

        Long caId = null;

        {
            ds.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(ds);

            Command cmd = new Command();
            CommandAction ca = new CommandAction.Builder(ptxBuilder.getPtx(), cmd, CA_CSV).build();

            PathAction pa1 = buildPathActionFromBuffer(BUF1);
            PathAction pa2 = buildPathActionFromBuffer(BUF2);
            PathAction pa3 = buildPathActionFromBuffer(BUF3);
            PathAction pa4 = buildPathActionFromBuffer(BUF4);

            ca.addPathAction(pa1);
            ca.addPathAction(pa2);
            ca.addPathAction(pa3);
            ca.addPathAction(pa4);

            ptxBuilder.setCommandAction(ca);
            caId = ca.getId();

            ds.commitTransaction();
        }

        ds.getEntityManager().clear();

        {
            assertNotNull(caId);

            ds.beginTransaction();

            CommandAction retrievedCa = ds.find(CommandAction.class, caId);
            for (PathAction pa : retrievedCa.getPrereqs()) {
                assertNotNull(pa.getId());
            }

            assertEquals(4, retrievedCa.getPrereqs().size());

            ds.commitTransaction();
        }
    }

    private PathAction buildPathActionFromBuffer(String buf) {

        int divider = 0;
        for (int i = 0; i < 10; i++) {
            divider = buf.indexOf(Constants.FS1, divider + 1);
        }
        final String pabuf = buf.substring(0, divider);
        final String psbuf = buf.substring(divider + 1);

        final PathState ps = new PathState.Builder(psbuf).build();
        final PathAction pa = new PathAction.Builder(ps, pabuf).build();

        return pa;
    }
}
