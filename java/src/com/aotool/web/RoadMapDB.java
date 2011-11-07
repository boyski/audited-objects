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

import java.io.File;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

import org.apache.log4j.Logger;

import com.aotool.Constants;
import com.aotool.entity.Command;
import com.aotool.entity.CommandAction;
import com.aotool.entity.PathAction;
import com.aotool.entity.PathState;
import com.aotool.entity.Ptx;
import com.strangegizmo.cdb.CdbMake;

public class RoadMapDB {

    private static final Logger logger = Logger.getLogger(RoadMapDB.class);

    private static final char PRQ = '<';
    private static final char TGT = '>';

    private static final String PTX_PREFIX = "X";
    private static final String STATE_PREFIX = "";
    private static final String CMD_PREFIX = "C";
    private static final String CHARSET = "UTF-8";
    private static final int ROADMAP_RADIX = 36;

    /**
     * Number of milliseconds to keep a PTX cached (optimization).
     */
    private static final long PTX_EXPIRATION_TIME = 24 * 60 * 60 * 1000L;

    // This doesn't have to be a TreeMap; it could be a HashMap. Debugging
    // is easier when it's ordered, that's all.
    private final TreeMap<Command, Map<Character, PathStateToPtx>> cmds = new TreeMap<Command, Map<Character, PathStateToPtx>>();
    private final Map<Ptx, Date> knownPtxes = new HashMap<Ptx, Date>();
    private final Set<Command> hasKids = new HashSet<Command>();

    public void addPtx(Ptx ptx) {
        /*
         * Since PTXes are static once finished, we can maintain a set of
         * "cached" PTXes for which calculations have already been done. This
         * should save some work at the cost of some memory. Note that there's a
         * difference between the set of cached PTXes and the set a particular
         * user wants to know about for a particular build. Even when the PTX
         * was previously in cache, we update the date associated with it
         * whenever it's re-used. This allows us to expire PTXes which haven't
         * been used in a while.
         */
        if (knownPtxes.containsKey(ptx)) {
            knownPtxes.put(ptx, new Date());
            return;
        } else {
            knownPtxes.put(ptx, new Date());
            if (logger.isDebugEnabled()) {
                logger.debug("Added Ptx" //
                        + ", ptxId = " + ptx.getIdString());
            }
        }

        Map<String, Command> ccodeMap = new HashMap<String, Command>();
        for (CommandAction cmdAction : ptx.getCommandActions()) {
            Command cmd = cmdAction.getCommand();
            Map<Character, PathStateToPtx> psMap;
            PathStateToPtx prqToPtx;
            PathStateToPtx tgtToPtx;
            if (cmds.containsKey(cmd)) {
                psMap = cmds.get(cmd);
            } else {
                psMap = new HashMap<Character, PathStateToPtx>();
                psMap.put(PRQ, new PathStateToPtx());
                psMap.put(TGT, new PathStateToPtx());
                cmds.put(cmd, psMap);
                ccodeMap.put(cmd.getCcode(), cmd);
            }

            prqToPtx = psMap.get(PRQ);
            tgtToPtx = psMap.get(TGT);

            for (PathAction pa : cmdAction.getPathActions()) {
                PathState ps = pa.getPathState();
                if (pa.isUnlink()) {
                    continue;
                }
                if (pa.isTarget()) {
                    tgtToPtx.add(ps, ptx);
                } else {
                    prqToPtx.add(ps, ptx);
                }
            }
        }

        /*
         * For every command we need to tell the client whether or not it has
         * children. Since the set of commands may change with each PTX we need
         * to re-determine this, as efficiently as possible, each time the set
         * changes.
         */
        for (Command cmd : cmds.keySet()) {
            String pccode = cmd.getPccode();
            if (pccode != null) {
                Command parent = ccodeMap.get(pccode);
                if (parent != null) {
                    hasKids.add(parent);
                }
            }
        }
    }

    public void expireOldPtxes() {
        long now = new Date().getTime();
        // This List needed to avoid ConcurrentModificationException.
        ArrayList<Ptx> ptxList = new ArrayList<Ptx>(knownPtxes.keySet());
        for (Ptx ptx : ptxList) {
            long lastUsed = knownPtxes.get(ptx).getTime();
            if (now - lastUsed > PTX_EXPIRATION_TIME) {
                expirePtx(ptx);
            }
        }
    }

    private void expirePtx(Ptx ptx) {
        // This List needed to avoid ConcurrentModificationException.
        List<Command> commandList = new ArrayList<Command>(cmds.keySet());
        for (Command cmd : commandList) {
            Map<Character, PathStateToPtx> psMap = cmds.get(cmd);
            PathStateToPtx prqToPtx = psMap.get(PRQ);
            PathStateToPtx tgtToPtx = psMap.get(TGT);
            prqToPtx.remove(ptx);
            tgtToPtx.remove(ptx);
            if (prqToPtx.isEmpty() && tgtToPtx.isEmpty()) {
                cmds.remove(cmd);
            }
        }

        knownPtxes.remove(ptx);
        if (logger.isDebugEnabled()) {
            logger.debug("Expired Ptx" //
                    + ", ptxId = " + ptx.getIdString());
        }
    }

    public void generate(File outputFile, Set<Ptx> chosen) throws IOException {

        CdbMake cdb = new CdbMake();
        cdb.start(outputFile.getAbsolutePath());

        addComment(cdb, "KEY-VALUE DATABASE TO SUPPORT AO SHOPPING");
        addComment(cdb, "");
        addComment(cdb, "KNOWN PTXES:");
        addComment(cdb, " key=(PTX INDEX) value=(PTX ID)");
        addComment(cdb, "");
        Map<Ptx, String> indexPtx = new LinkedHashMap<Ptx, String>();
        int ptxix = 0;
        for (Ptx ptx : chosen) {
            String key = PTX_PREFIX + ptxix++;
            StringBuilder sb = new StringBuilder();
            sb.append(key);
            sb.append('=');
            sb.append(ptx.getIdString());
            addToRoadMap(cdb, PTX_PREFIX, sb.toString());
            indexPtx.put(ptx, key);
        }

        addComment(cdb, "");
        addComment(cdb, "KNOWN PATHSTATES:");
        addComment(cdb, "key=(PS INDEX) value=(PS CSV)");
        addComment(cdb, "");
        int psix = 0;
        Map<PathState, String> indexPS = new LinkedHashMap<PathState, String>();
        for (Map<Character, PathStateToPtx> psMap : cmds.values()) {
            PathStateToPtx prqToPtx = psMap.get(PRQ);
            if (prqToPtx != null) {
                for (PathState ps : prqToPtx.getPathStateSet()) {
                    if (!indexPS.containsKey(ps)) {
                        String ix = STATE_PREFIX + Integer.toString(psix++, ROADMAP_RADIX);
                        indexPS.put(ps, ix);
                    }
                }
            }
            PathStateToPtx tgtToPtx = psMap.get(TGT);
            if (tgtToPtx != null) {
                for (PathState ps : tgtToPtx.getPathStateSet()) {
                    if (!indexPS.containsKey(ps)) {
                        String ix = STATE_PREFIX + Integer.toString(psix++, ROADMAP_RADIX);
                        indexPS.put(ps, ix);
                    }
                }
            }
        }

        for (PathState ps : indexPS.keySet()) {
            StringBuilder sb = new StringBuilder();
            sb.append(ps.toCSVString());
            sb.append(Constants.FS1);
            sb.append(ps.getPathString());
            addToRoadMap(cdb, indexPS.get(ps), sb.toString());
        }

        addComment(cdb, "");
        addComment(cdb, "MAPPINGS BETWEEN PATHSTATES AND COMMANDS, IN BLOCKS OF:");
        addComment(cdb, "key=(COMMAND LINE) value=(CMDIX)");
        addComment(cdb, "key=(CMDIX)  value=(PCCODE,PATHCODE,HAS_TGT,AGG,KIDS,DURATION,RWD)");
        addComment(cdb, "key=<(CMDIX) value=(PREREQ-LIST),(PTX-LIST)");
        addComment(cdb, "key=>(CMDIX) value=(TARGET-LIST),(PTX-LIST)");
        addComment(cdb, "");
        int cmdix = 0;
        for (Command cmd : cmds.keySet()) {
            String cix = CMD_PREFIX + Integer.toString(cmdix++, ROADMAP_RADIX);

            addToRoadMap(cdb, cmd.getOriginalLine(), cix);

            StringBuilder cssb = new StringBuilder();

            /*
             * The parent ccode. May be used to distinguish commands which are
             * textually identical. For example a command like "rm -f core" or
             * "date >> log" could easily occur in multiple places within a
             * build referring to different files "core" or "log".
             */
            cssb.append(cmd.getPccode()).append(Constants.FS1);

            /*
             * The pathcode is not strictly necessary for shopping but it's a
             * useful check because we can compare the actual pathcode derived
             * during shopping with the "remembered" pathcode. This could be
             * removed once stable.
             */
            cssb.append(cmd.getPathCode()).append(Constants.FS1);

            /*
             * Does it create or modify any files? If not, recycling makes no
             * sense.
             */
            boolean has_tgt = false;
            Map<Character, PathStateToPtx> xx = cmds.get(cmd);
            if (xx != null) {
                PathStateToPtx yy = xx.get(TGT);
                if (yy != null) {
                    has_tgt = !yy.isEmpty();
                }
            }
            cssb.append(Boolean.valueOf(has_tgt)).append(Constants.FS1);

            /*
             * Is it an aggregated command or a leaf node?
             */
            cssb.append(Boolean.valueOf(cmd.isAggregated())).append(Constants.FS1);

            /*
             * Indicates whether the command has any non-aggregated children.
             * Recycling a command with such children is challenging as we would
             * have to reproduce the results of all descendants too.
             */
            if (hasKids.contains(cmd)) {
                cssb.append('+').append(Constants.FS1);
            } else {
                cssb.append('-').append(Constants.FS1);
            }

            /*
             * Duration. Currently not in use. The idea would be that the client
             * could look at the duration and not bother recycling commands
             * which are fast enough to be insignificant and/or where
             * downloading target data would take longer than regenerating it.
             * See WI #175.
             */
            cssb.append('0').append(Constants.FS1);

            /*
             * We may not actually need the RWD here; it is not used to
             * distinguish commands. But it can be helpful during debugging to
             * see which commands were run in what dir (assuming it's a
             * recursive build being debugged). Anyway, this could probably go
             * away to save CDB space though it would require a related change
             * on the client side.
             */
            cssb.append(cmd.getRelativeWorkingDirectory());

            addToRoadMap(cdb, cix, cssb.toString());

            /*
             * Spit out prereq and target mappings in turn.
             */
            Map<Character, PathStateToPtx> psMap = cmds.get(cmd);
            if (psMap != null) {
                mapPathStates(cdb, psMap, PRQ, cix, indexPtx, indexPS);
                mapPathStates(cdb, psMap, TGT, cix, indexPtx, indexPS);
            }
        }

        cdb.finish();
    }

    private void mapPathStates(CdbMake cdb, Map<Character, PathStateToPtx> psMap, Character type,
            String cix, Map<Ptx, String> indexPtx, Map<PathState, String> indexPS)
            throws IOException {

        PathStateToPtx psToPtx = psMap.get(type);
        assert (psToPtx != null);
        Map<String, StringBuilder> backMap = new HashMap<String, StringBuilder>();

        for (PathState ps : psToPtx.getPathStateSet()) {

            StringBuilder ptxBuilder = new StringBuilder();
            for (Ptx ptx : psToPtx.getPtxSet(ps)) {
                if (ptxBuilder.length() > 0) {
                    ptxBuilder.append(Constants.FS2);
                }
                ptxBuilder.append(indexPtx.get(ptx));
            }

            String ptxList = ptxBuilder.toString();
            if (backMap.containsKey(ptxList)) {
                StringBuilder cmdList = backMap.get(ptxList);
                cmdList.append(Constants.FS2);
                cmdList.append(indexPS.get(ps));
            } else {
                StringBuilder cmdList = new StringBuilder();
                cmdList.append(indexPS.get(ps));
                backMap.put(ptxList, cmdList);
            }
        }

        for (String ptxList : backMap.keySet()) {
            StringBuilder cmdList = backMap.get(ptxList);
            String key = type + cix;
            StringBuilder value = new StringBuilder();
            value.append(cmdList);
            value.append(Constants.FS1);
            value.append(ptxList);
            addToRoadMap(cdb, key, value.toString());
        }
    }

    private void addComment(CdbMake cdb, String comment) throws UnsupportedEncodingException,
            IOException {
        addToRoadMap(cdb, "#", comment);
    }

    private void addToRoadMap(CdbMake cdb, String key, String value)
            throws UnsupportedEncodingException, IOException {
        cdb.add(key.getBytes(CHARSET), value.getBytes(CHARSET));
        if (logger.isDebugEnabled()) {
            logger.debug("addToRoadMap" //
                    + ", key = [" + key + "]" //
                    + ", value = [" + value + "]"); //$NON-NLS-1$
        }
    }

    @Override
    public String toString() {
        return "RoadMapDB [knownPTXes=" + knownPtxes.toString() + "]";
    }

    private static class PathStateToPtx {

        private final Map<PathState, HashSet<Ptx>> ps2ptx;

        private PathStateToPtx() {
            ps2ptx = new HashMap<PathState, HashSet<Ptx>>();
        }

        private void add(PathState ps, Ptx ptx) {
            HashSet<Ptx> ptxSet = ps2ptx.get(ps);
            if (ptxSet == null) {
                ptxSet = new HashSet<Ptx>();
                ps2ptx.put(ps, ptxSet);
            }
            ptxSet.add(ptx);
        }

        private Set<PathState> getPathStateSet() {
            return ps2ptx.keySet();
        }

        private HashSet<Ptx> getPtxSet(PathState ps) {
            return ps2ptx.get(ps);
        }

        private boolean isEmpty() {
            return ps2ptx.isEmpty();
        }

        private void remove(Ptx ptx) {
            // This List needed to avoid ConcurrentModificationException.
            ArrayList<PathState> al = new ArrayList<PathState>(ps2ptx.keySet());
            for (PathState ps : al) {
                HashSet<Ptx> ptxSet = ps2ptx.get(ps);
                ptxSet.remove(ptx);
                if (ptxSet.isEmpty()) {
                    ps2ptx.remove(ps);
                }
            }
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            for (PathState ps : ps2ptx.keySet()) {
                sb.append(ps).append(':').append('\n');
                HashSet<Ptx> ptxset = ps2ptx.get(ps);
                for (Ptx ptx : ptxset) {
                    sb.append('\t');
                    sb.append(ptx.getIdString());
                    sb.append('\n');
                }
            }
            return sb.toString();
        }
    }
}
