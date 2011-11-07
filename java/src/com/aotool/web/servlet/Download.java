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

import java.io.BufferedInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.zip.GZIPInputStream;

import javax.servlet.ServletException;
import javax.servlet.ServletOutputStream;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.log4j.Logger;

import com.aotool.entity.DataService;
import com.aotool.entity.DataServiceHelper;
import com.aotool.entity.PathState;
import com.aotool.entity.Project;
import com.aotool.util.FileDataContainer;
import com.aotool.web.AppProperties;
import com.aotool.web.Http;

/**
 * The Download servlet delivers the contents of a file container from the
 * server to a client which requests it. When an audited command on the client
 * has completed the shopping operation and determined that it can reuse a
 * previous file, it will request those contents (along with metadata such as
 * permissions, datestamp, etc) from the server via this servlet.
 */
public final class Download extends AOServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(Download.class);

    @Override
    public void doGet(HttpServletRequest req, HttpServletResponse res) throws ServletException,
            IOException {

        DataService dataService = null;

        try {
            dataService = DataServiceHelper.create(getServletContext());
            dataService.beginTransaction();

            String projectName = req.getParameter(Http.PROJECT_NAME_PARAM);
            Project project = dataService.find(Project.class, projectName);

            String csv = req.getParameter(Http.PS_CSV_PARAM);
            PathState ps = new PathState.Builder(csv).build();

            String content_disposition = String.format("inline; moment=\"%s\" mode=\"%s\"", //$NON-NLS-1$
                    ps.getTimeStamp(), ps.getModeString());
            res.setHeader(Http.CONTENT_DISPOSITION_HEADER, content_disposition);
            res.setContentType(Http.APPLICATION_OCTET_STREAM);

            /*
             * Special case: we don't bother to upload and store empty files;
             * all empty files look the same anyway. But that means we can't go
             * looking for an empty container at download time either. We could
             * also short-circuit the download of null files on the client side,
             * which would be faster, but only the server side knows the
             * metadata like timestamp and mode which even empty files have. So
             * currently we go through the motions of downloading empty files
             * just to get the metadata. An alternative might be to put the
             * metadata in the roadmap but realistically, there are more
             * fruitful avenues for optimization.
             */
            if (ps.getSize() > 0) {
                File rootDir = AppProperties.getContainerRootDir();
                FileDataContainer container = new FileDataContainer(rootDir, project, ps);
                InputStream in = container.getStream();

                /*
                 * The client is allowed to ask the server to do compression for
                 * it. By default, compression/decompression of containers is
                 * done by the client. Containers are always *stored* in
                 * compressed format.
                 */
                boolean uncompressed = req.getParameter(Http.UNCOMPRESSED_TRANSFERS_PARAM) != null;
                if (uncompressed) {
                    in = new GZIPInputStream(in);
                } else {
                    res.setHeader(Http.CONTENT_ENCODING_HEADER, Http.GZIP_ENCODING);
                }

                // Wrap a buffer around it
                in = new BufferedInputStream(in);

                ServletOutputStream out = res.getOutputStream();

                byte[] buf = new byte[Http.BUFFER_SIZE];
                int len;
                while ((len = in.read(buf, 0, buf.length)) != -1) {
                    out.write(buf, 0, len);
                }
                in.close();

                if (logger.isInfoEnabled()) {
                    logger.info("Recycled" //
                            + ", container = " + container.toStandardString()); //$NON-NLS-1$
                }
            }

            dataService.commitTransaction();

        } catch (Exception e) {
            DataServiceHelper.rollbackTx(dataService);
            throw new ServletException(e);
        } finally {
            DataServiceHelper.close(dataService);
        }
    }
}
