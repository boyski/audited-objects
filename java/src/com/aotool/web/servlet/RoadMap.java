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

package com.aotool.web.servlet;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.zip.GZIPOutputStream;

import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.log4j.Logger;

import com.aotool.Constants;
import com.aotool.entity.DataService;
import com.aotool.entity.DataServiceHelper;
import com.aotool.entity.Project;
import com.aotool.entity.Ptx;
import com.aotool.web.Http;
import com.aotool.web.RoadMapDB;

/**
 * The RoadMap servlet is used at the beginning of a PTX to generate a
 * <i>compact</i> description of prior PTXes within the same project and send it
 * over to the client. This description file is called the <i>roadmap</i>. It is
 * used for "shopping", which is the act of looking for previously-generated
 * files which can be reused in order to avoid having to regenerate the same
 * data.
 * <p>
 * </p>
 * <b>This servlet should be called <i>before</i> a PTX or session is
 * created.</b>
 * <p>
 * </p>
 * The term <i>compact</i> above is important because the roadmap has the
 * potential to become large enough that transferring it to the client system
 * can cause a noticeable lag. Therefore, we go to great lengths to keep things
 * as small as possible. In particular, items like PTX IDs, pathnames, and
 * command lines are issued index numbers and referred to subsequently by those
 * numbers. This makes the format somewhat more cryptic but still
 * human-readable, and seems to have largely eliminated the size problem.
 */
public final class RoadMap extends AOServlet {

    private static final long serialVersionUID = 1L;

    private static final String ROADMAPDB_ATTR = "ROADMAPDB";

    private static final Logger logger = Logger.getLogger(RoadMap.class);

    @Override
    public void doGet(HttpServletRequest req, HttpServletResponse response)
            throws ServletException, IOException {

        DataService dataService = null;
        Project project = null;
        try {
            dataService = DataServiceHelper.create(getServletContext());
            dataService.beginTransaction();

            // Figure out the project name and use it to get the Project object.
            String projectName = req.getParameter(Http.PROJECT_NAME_PARAM);
            project = dataService.find(Project.class, projectName);
            if (project == null) {
                if (logger.isInfoEnabled()) {
                    logger.info("No roadmap generation, returning empty response"); //$NON-NLS-1$}
                }
            } else {

                String logName = req.getParameter(Http.LOGIN_NAME_PARAM);
                String hostName = req.getParameter(Http.HOST_NAME_PARAM);
                String strategy = req.getParameter(Http.PTX_STRATEGY_PARAM);

                Set<Ptx> ptxes = new LinkedHashSet<Ptx>();

                preparePtxes(project, logName, hostName, strategy, ptxes);
                if (logger.isDebugEnabled()) {
                    logger.debug("Prepared ptxes" + ", values = " + ptxes);
                }

                if (!ptxes.isEmpty()) {
                    sendRoadMap(response, project, ptxes);
                } else {
                    /*
                     * Send a zero-length document if there's not going to be
                     * anything worthwhile in it anyway. The client can use the
                     * zero size as a flag and avoid cranking up the shopper at
                     * all.
                     */
                    if (ptxes.isEmpty()) {
                        if (logger.isInfoEnabled()) {
                            logger.info("Skipping roadmap generation"); //$NON-NLS-1$
                        }
                    }
                }

                dataService.commitTransaction();
            }
        } catch (Exception e) {
            DataServiceHelper.rollbackTx(dataService);
            throw new ServletException(e);
        } finally {
            DataServiceHelper.close(dataService);
        }
    }

    private void preparePtxes(Project project, String logName, String hostName, String strategy,
            Set<Ptx> chosenPtxes) throws IOException {

        // The numbers of PTXes to select:
        // - by current user
        // - from current (client)
        // - host
        // - in total.
        int max_ptx_total;
        int max_ptx_label;
        int max_ptx_user;
        int max_ptx_host;
        max_ptx_total = max_ptx_label = max_ptx_user = max_ptx_host = -1;

        if (strategy != null) {
            String[] maxima = strategy.split(Constants.FS1);

            switch (maxima.length) {
            case 4:
                max_ptx_host = Integer.parseInt(maxima[3]);
            case 3:
                max_ptx_user = Integer.parseInt(maxima[2]);
            case 2:
                max_ptx_label = Integer.parseInt(maxima[1]);
            case 1:
                max_ptx_total = Integer.parseInt(maxima[0]);
                break;
            }
        }

        /*
         * Rather than dump all known PTXes, which would have scale issues, we
         * have limits on how many are offered. The algorithm is currently: the
         * most recent U from the current user, the most recent L which are
         * labeled, the most recent H from the current host machine, and up to T
         * in total. Any of these may be made unlimited by setting the maximum
         * to the value -1.
         */
        int ptx_total = 0;
        int ptx_label = 0;
        int ptx_host = 0;
        int ptx_user = 0;
        for (Ptx ptx : project.getPtxList()) {

            if (ptx.getUploadedCount() == 0 || !ptx.isDone()) {
                continue;
            }

            chosenPtxes.add(ptx);

            if (ptx.isLabeled()) {
                ptx_label++;
            }
            if (ptx.getLogName().equals(logName)) {
                ptx_user++;
            }
            if (ptx.getHostName().equals(hostName)) {
                ptx_host++;
            }
            ptx_total++;

            if ((max_ptx_label >= 0 && ptx_label >= max_ptx_label)
                    && (max_ptx_user >= 0 && ptx_user >= max_ptx_user)
                    && (max_ptx_host >= 0 && ptx_host >= max_ptx_host)) {
                if (ptx_total >= max_ptx_total) {
                    break;
                }
            }
        }
    }

    private void sendRoadMap(HttpServletResponse response, Project project, Set<Ptx> chosenPtxes)
            throws IOException {

        OutputStream outputStream = response.getOutputStream();

        /*
         * Now we know we're definitely sending a roadmap file. Mark it as
         * compressed.
         */
        response.setHeader(Http.CONTENT_ENCODING_HEADER, Http.GZIP_ENCODING);

        if (logger.isInfoEnabled()) {
            logger.info("Beginning roadmap generation"); //$NON-NLS-1$
        }

        /*
         * Here we cache the roadmap data in application context so it doesn't
         * need to be remade from scratch each time; instead, we only need to
         * add previously unseen PTXes to it.
         */
        RoadMapDB rdb = getRoadMapDB(getServletContext(), project);
        for (Ptx ptx : chosenPtxes) {
            rdb.addPtx(ptx);
        }

        /*
         * Trim any cached PTXes which haven't been used in a while. The
         * definition of "a while" is pretty simplistic right now. This
         * "pruning" feature could be enhanced.
         */
        rdb.expireOldPtxes();

        if (logger.isDebugEnabled()) {
            logger.debug("RoadMapDB ready"); //$NON-NLS-1$
        }

        /*
         * IDEA - if roadmap generation time becomes significant, look into
         * modifying sg-cdb to generate it into memory directly.
         */

        // Work out a unique path for the CDB temp file.
        File dir = (File) getServletContext().getAttribute("javax.servlet.context.tempdir");
        long threadId = Thread.currentThread().getId();
        /* Make the temp file name thread safe */
        String rbase = "roadmap." + String.valueOf(threadId);
        File tempRoadMapFile = File.createTempFile(rbase, ".cdb", dir);

        rdb.generate(tempRoadMapFile, chosenPtxes);
        long size = tempRoadMapFile.length();

        if (logger.isDebugEnabled()) {
            logger.debug("RoadMapDB generated" + ", size = " + size); //$NON-NLS-1$
        }

        FileInputStream fis = new FileInputStream(tempRoadMapFile);
        GZIPOutputStream gzos = new GZIPOutputStream(outputStream);
        byte[] buf = new byte[Http.BUFFER_SIZE];
        int len;
        while ((len = fis.read(buf, 0, buf.length)) != -1) {
            gzos.write(buf, 0, len);
        }
        gzos.close();
        fis.close();

        if (!tempRoadMapFile.delete()) {
            tempRoadMapFile.deleteOnExit();
        }

        if (logger.isInfoEnabled()) {
            logger.info("Delivered roadmap.cdb" + ", size = " + size);
        }
    }

    @SuppressWarnings("unchecked")
    private static RoadMapDB getRoadMapDB(ServletContext ctx, Project project) {
        HashMap<String, RoadMapDB> rdbMap = (HashMap<String, RoadMapDB>) ctx
                .getAttribute(ROADMAPDB_ATTR);
        if (rdbMap == null) {
            rdbMap = new HashMap<String, RoadMapDB>();
        }
        RoadMapDB rdb = rdbMap.get(project.getName());
        if (rdb == null) {
            rdb = new RoadMapDB();
            rdbMap.put(project.getName(), rdb);
            if (logger.isDebugEnabled()) {
                logger.debug("Created new RoadMapDB"); //$NON-NLS-1$
            }
        } else {
            if (logger.isDebugEnabled()) {
                logger.debug("Using cached RoadMapDB"); //$NON-NLS-1$
            }
        }
        return rdb;
    }

    private static void deleteRoadMapDB(ServletContext ctx) {
        ctx.removeAttribute(ROADMAPDB_ATTR);
        if (logger.isInfoEnabled()) {
            logger.info("Emptied roadmap cache"); //$NON-NLS-1$
        }
    }

    public static void scrub(ServletContext ctx) {
        deleteRoadMapDB(ctx);
    }
}
