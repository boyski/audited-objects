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

package com.aotool.util;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.util.zip.GZIPOutputStream;

import org.apache.log4j.Logger;

import com.aotool.entity.PathState;
import com.aotool.entity.Project;
import com.aotool.entity.Ptx;
import com.aotool.web.Http;
import com.aotool.web.servlet.Session;

/**
 * The FileDataContainer class manages the contents of any client-side files
 * uploaded to the server. This is the only class allowed to interact directly
 * with the local (server) filesystem. The idea is that, if at any point it's
 * determined worthwhile to keep client file data in the persistence store
 * rather than the server filesystem, only this class would need to be modified.
 */
public final class FileDataContainer {

    private static final Logger logger = Logger.getLogger(Session.class);

    /** A conventional pathname component under which logfiles are stored. */
    private static final String LOGBASE = "_LOGFILES_"; //$NON-NLS-1$

    /** The full path to the container. */
    private File container;

    /**
     * Instantiates a contents container for a logfile.
     * 
     * @param project
     *            the Project
     * @param ptx
     *            the PTX
     */
    public FileDataContainer(File rootDir, Project project, Ptx ptx) {
        File projectDir = new File(rootDir, project.getName());
        File basedir = new File(projectDir, LOGBASE);
        container = new File(basedir, ptx.getIdString() + ".log.gz");
    }

    /**
     * Instantiates a contents container for an uploaded file.
     * 
     * It would be a little more efficient to not segregate containers by
     * project and basename; this would allow containers with identical contents
     * to be shared regardless of path. However, the savings would be minor and
     * segregating by project makes it easier to remove old projects and reclaim
     * their space. Segregating by basename is just a debugging convenience.
     * 
     * @param rootdir
     *            the root directory of all containers
     * @param project
     *            the Project
     * @param ps
     *            a PathState object to be saved
     */
    public FileDataContainer(File rootDir, Project project, PathState ps) {
        String pname = ps.getPathString();
        String unencoded = new File(pname).getName();
        String encoded;
        try {
            encoded = java.net.URLEncoder.encode(unencoded, "UTF-8"); //$NON-NLS-1$
        } catch (UnsupportedEncodingException e) {
            // We feel confident that UTF-8 will always be supported.
            encoded = unencoded;
        }
        /*
         * Special case - there is one character ('*') which URL encoding
         * doesn't handle and which is illegal in some (Windows) pathnames.
         */
        encoded = encoded.replace("*", "%2A"); //$NON-NLS-1$ //$NON-NLS-2$
        File projectDir = new File(rootDir, project.getName());
        File basedir = new File(projectDir, encoded);
        StringBuilder sb = new StringBuilder();
        sb.append(ps.getSize());
        sb.append('.');
        sb.append(ps.getDataCode());
        sb.append(".gz"); //$NON-NLS-1$
        container = new File(basedir, sb.toString());
    }

    /**
     * Creates a directory tree if not already present. MUST be synchronized to
     * avoid mkdir race condition. This method exists only to synchronize on.
     * 
     * @param path
     *            the directory path to create
     */
    private static synchronized void mkdirs(File path) {
        if (!path.exists()) {
            path.mkdirs();
        }
    }

    /**
     * Takes an input stream and stores all data coming from that stream as a
     * file contents container.
     * 
     * @param bin
     *            a BufferedInputStream containing the file data
     * @param gzipped
     *            is this data already gzipped?
     * 
     * @throws IOException
     *             Signals that an I/O exception has occurred.
     */
    public void storeStream(BufferedInputStream bin, boolean gzipped) throws IOException {
        /*
         * Ensure that the required directory tree is present.
         */
        mkdirs(container.getParentFile());

        OutputStream fos = new FileOutputStream(container);

        /*
         * If the incoming data isn't already gzipped we have to gzip it here
         * because containers are always stored in compressed format.
         */
        if (!gzipped) {
            fos = new GZIPOutputStream(fos);
        }
        /*
         * AFAICT it's always considered good to use this even though
         * GZipOutputStream has its own buffer.
         */
        fos = new BufferedOutputStream(fos);

        byte[] buf = new byte[Http.BUFFER_SIZE];
        int len;
        while ((len = bin.read(buf, 0, buf.length)) != -1) {
            fos.write(buf, 0, len);
        }
        fos.close();
    }

    /**
     * Returns a buffered stream from which the data of this container may be
     * read. This data is required to be in gzip format.
     * 
     * @return the compressed data stream
     * 
     * @throws IOException
     *             Signals that an I/O exception has occurred.
     */
    public BufferedInputStream getStream() throws IOException {
        InputStream in = new FileInputStream(container);
        BufferedInputStream bin = new BufferedInputStream(in);
        return bin;
    }

    /**
     * Gets the standard string value of this container.
     * 
     * @return the standard string value
     */
    public String toStandardString() {
        return container.getPath();
    }

    @Override
    public String toString() {
        return this.toStandardString();
    }

    /**
     * Does this container currently exist?
     * 
     * @return true, if container exists
     */
    public boolean exists() {
        return container.exists();
    }

    /**
     * Delete this container.
     */
    public void delete() {
        container.delete();
    }

    /**
     * Delete all containers for the specified project.
     */
    public static void deleteProjectContainers(File rootDir, Project project) {
        File projectDir = new File(rootDir, project.getName());
        if (logger.isInfoEnabled()) {
            logger.info("Removing container directory " + projectDir.getPath());
        }
        removeDir(projectDir);
    }

    /**
     * Removes the specified directory and all its contents.
     * 
     * @param dir
     *            the directory to be removed
     */
    public static void removeDir(File dir) {
        String[] list = dir.list();
        if (list == null) {
            list = new String[0];
        }
        for (int i = 0; i < list.length; i++) {
            File f = new File(dir, list[i]);
            if (f.isDirectory()) {
                removeDir(f);
            } else {
                if (!f.delete()) {
                    // Deferring deletion of file if need be.
                    // This actually didn't seem to work last I looked.
                    f.deleteOnExit();
                }
            }
        }
        if (dir.exists() && !dir.delete()) {
            // Vide ultra.
            dir.deleteOnExit();
        }
    }
}
