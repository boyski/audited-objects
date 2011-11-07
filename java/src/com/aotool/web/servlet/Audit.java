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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.zip.GZIPInputStream;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import org.apache.log4j.Logger;

import com.aotool.Constants;
import com.aotool.Messages;
import com.aotool.entity.Command;
import com.aotool.entity.CommandAction;
import com.aotool.entity.DataService;
import com.aotool.entity.DataServiceHelper;
import com.aotool.entity.PathAction;
import com.aotool.entity.PathState;
import com.aotool.entity.Ptx;
import com.aotool.entity.PtxBuilder;
import com.aotool.web.Http;
import com.aotool.web.SessionHelper;

/**
 * The Audit servlet accepts an "audit packet" from the client, factors it out
 * into the server CommandAction abstraction, and persists the resulting state.
 * It will be invoked by the client once per aggregated command invocation.
 */
public final class Audit extends AOServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(Audit.class);

    @Override
    public void doPost(HttpServletRequest request, HttpServletResponse response)
            throws IOException, ServletException {

        HttpSession session = request.getSession(false);
        SessionHelper sessionHelper = new SessionHelper(session);
        DataService dataService = null;
        PrintWriter out = response.getWriter();

        try {
            dataService = DataServiceHelper.create(getServletContext());
            dataService.beginTransaction();

            PtxBuilder ptxBuilder = new PtxBuilder(dataService, sessionHelper.getCurrentPtxId());

            BufferedReader reader;
            boolean gzipped = request.getHeader(Http.X_GZIPPED_HEADER) != null;
            if (gzipped) {
                InputStream in = request.getInputStream();
                in = new GZIPInputStream(in);
                reader = new BufferedReader(new InputStreamReader(in));
            } else {
                reader = request.getReader();
            }

            CommandAction commandAction = processAudit(ptxBuilder.getPtx(), reader);

            reader.close();

            if (commandAction != null) {
                if (logger.isInfoEnabled()) {
                    logger.info(commandAction.isRecycled() ? "Recycling" : "Auditing" //
                            + ", command = " + commandAction.getCommand().getLine());
                }

                ptxBuilder.setCommandAction(commandAction);

                if (logger.isInfoEnabled()) {
                    logger.info(commandAction.isRecycled() ? "Recycled" : "Audited" //
                            + ", command = " + commandAction.getCommand().getLine());
                }
            } else {

                out.print(Http.errorMessage(Messages.getString("Audit.0"))); //$NON-NLS-1$
            }

            dataService.commitTransaction();

        } catch (Exception e) {
            DataServiceHelper.rollbackTx(dataService);
            throw new ServletException(e);
        } finally {
            DataServiceHelper.close(dataService);
        }
    }

    /**
     * Returns the index of the nth comma in the supplied CSV string.
     * 
     * @param buf
     *            the String buffer
     * @param n
     *            the number of the comma searched for
     * 
     * @return the index of the specified comma
     */
    private static int findDivider(String buf, int n) {
        int divider = 0;
        for (int i = 0; i < n; i++) {
            divider = buf.indexOf(Constants.FS1, divider + 1);
        }
        return divider;
    }

    /**
     * Processes the audit record of one Command and returns a CommandAction.
     * From the supplied Reader it takes a series of CSV lines describing the
     * command and the state of each file it touched.
     * 
     * @param ptx
     *            the current PTX
     * @param in
     *            the input stream from which the audit record is read
     * 
     * @return a CommandAction object
     * 
     * @throws IOException
     *             Signals that an I/O exception has occurred.
     */
    private static CommandAction processAudit(Ptx ptx, BufferedReader in) throws IOException {

        Command command = null;
        String buf;
        CommandAction ca = null;

        while ((buf = in.readLine()) != null) {
            if (Character.isDigit(buf.charAt(0))) {
                if (command == null) {
                    // Since the CA and CMD data share a single CSV
                    // line, we need to break them apart to feed to
                    // constructors.
                    int divider = findDivider(buf, 7);
                    String cabuf = buf.substring(0, divider);
                    String cmdbuf = buf.substring(divider + 1);

                    command = new Command.Builder(cmdbuf).build();
                    ca = new CommandAction.Builder(ptx, command, cabuf).build();
                } else {
                    command.setAggregated();
                    // Currently we do not store aggregated subcommands
                    // but we could probably store and report them upon
                    // request.
                }
            } else if (Character.isLetter(buf.charAt(0))) {
                // Since the PA and PS data share a single CSV line,
                // we need to break them apart to feed to constructors.
                int divider = findDivider(buf, PathAction.Builder.NUMBER_OF_PATHACTION_FIELDS);
                String pabuf = buf.substring(0, divider);
                String psbuf = buf.substring(divider + 1);

                PathState ps = new PathState.Builder(psbuf).build();
                PathAction pa = new PathAction.Builder(ps, pabuf).build();

                ca.addPathAction(pa);
                
                if (ps.hasDataCode()) {
                    ptx.addDataCodeWidth(ps.getDataCode().length());
                }
            }
        }

        return ca;
    }
}
