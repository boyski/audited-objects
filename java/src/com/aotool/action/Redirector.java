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
package com.aotool.action;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

import javax.servlet.RequestDispatcher;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.log4j.Logger;

import com.aotool.action.support.ActionHelper;
import com.aotool.action.support.ClassGlob;
import com.aotool.entity.DataService;
import com.aotool.entity.DataServiceHelper;
import com.aotool.entity.Project;
import com.aotool.web.Http;

/**
 * This servlet is a little - ok, a lot - ugly. We'd like to avoid having to
 * make an entry in the deployment descriptor for every little action servlet;
 * it would make web.xml large and ugly and require restarting the server for
 * every change. Mapping via web.xml would also make it hard to make command
 * abbreviations such as "lsb" for "lsbuilds".
 * <p>
 * </p>
 * So instead we tell web.xml to map ALL action URLs to here, where we parse the
 * URL to figure out where it really wants to be and use some Reflection APIs to
 * send it there. It works but is extremely ugly. The next version of Java EE
 * may allow deployment instructions to be handled via code annotations instead
 * of web.xml but even that wouldn't allow for the automatic abbreviation
 * support offered here.
 */
public final class Redirector extends HttpServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(Redirector.class);

    private static final String ABOUT_ACTION_NAME = "About"; //$NON-NLS-1$
    private static final String ACTION_METHOD_NAME = "doAction"; //$NON-NLS-1$
    private static final String ADMIN_ACTION_NAME = "Admin"; //$NON-NLS-1$
    private static final String AMPERSAND = "&"; //$NON-NLS-1$
    private static final String CLASS_GLOB = ".*\\.class"; //$NON-NLS-1$
    private static final String CLASSES = "classes"; //$NON-NLS-1$
    private static final String DOC_METHOD_NAME = "getHelpMessage"; //$NON-NLS-1$
    private static final String DOT = "."; //$NON-NLS-1$
    private static final String HELP = "help"; //$NON-NLS-1$
    private static final String LS_PROJECTS_ACTION_NAME = "LsProjects"; //$NON-NLS-1$
    private static final String NULL_STRING = ""; //$NON-NLS-1$
    private static final String PLUS = "+"; //$NON-NLS-1$
    private static final String QUESTION_MARK = "?"; //$NON-NLS-1$
    private static final String SLASH = "/"; //$NON-NLS-1$
    private static final String SUMMARY_FORMAT = "%-12s - %s\n"; //$NON-NLS-1$
    private static final String SUMMARY_METHOD_NAME = "getSummary"; //$NON-NLS-1$
    private static final String TEMP_PROJECT_NAME = ".no_project"; //$NON-NLS-1$
    private static final String WEB_INF = "WEB-INF"; //$NON-NLS-1$

    @Override
    public void doGet(HttpServletRequest req, HttpServletResponse res) throws IOException,
            ServletException {

        DataService dataService = null;
        String action = null;
        String errmsg = null;
        ActionHelper ah = null;

        try {
            res.setContentType(Http.TEXT_PLAIN);

            dataService = DataServiceHelper.create(getServletContext());
            dataService.beginTransaction();

            /*
             * If there is no "current project", make a temporary one to be used
             * for this request cycle. Since we never persist it, it will go out
             * of scope after the request and disappear.
             */
            Project project = null;
            String projectName = req.getParameter(Http.PROJECT_NAME_PARAM);
            if (projectName == null) {
                projectName = TEMP_PROJECT_NAME;
            } else {
                project = dataService.find(Project.class, projectName);
            }

            if (project == null) {
                project = new Project(projectName);
            }

            String classname = null;
            Method domethod = null;

            // Analyze the URL to derive the action name.
            final String[] p3 = req.getPathInfo().split(SLASH, 3);
            action = p3[1];

            ah = new ActionHelper(action, req, res, getServletContext(), project, dataService);

            // Work out the physical filesystem path to the actions.
            final String pbase = getServletContext().getRealPath(SLASH);
            final String pkg = this.getClass().getPackage().getName();
            final String pkgdir = pkg.replace('.', '/');
            final File basedir = new File(pbase, WEB_INF);

            File clsdir = new File(basedir, CLASSES);
            clsdir = new File(clsdir, pkgdir);

            // Special case - handle "help" and "man" actions inline here.
            if (HELP.equals(action)) {
                // Create an ArrayList of action names passed in.
                ArrayList<String> arglist = null;
                ah.registerStandardFlags();
                ah.parseOptions();
                arglist = ah.getArguments();

                if (arglist.isEmpty()) {
                    PrintWriter out = res.getWriter();

                    // Get a list of all known action names.
                    ClassGlob glob = new ClassGlob(NULL_STRING, CLASS_GLOB);
                    String[] matches = clsdir.list(glob);
                    ArrayList<String> matchlist = new ArrayList<String>(Arrays.asList(matches));
                    Collections.sort(matchlist);

                    for (String match : matchlist) {
                        String cmd = match.toLowerCase().substring(0, match.indexOf('.'));
                        classname = pkg + DOT + match.substring(0, match.indexOf('.'));

                        // Try to get a class object for the named servlet.
                        Class<?> servclass = null;
                        try {
                            servclass = Class.forName(classname);
                        } catch (ClassNotFoundException e) {
                            // It's only help, after all.
                        }
                        if (servclass == null) {
                            continue;
                        }

                        // Only non-abstract, final classes are considered.
                        // We assume anything else is not exposed to the user.
                        final int mod = servclass.getModifiers();
                        if (Modifier.isAbstract(mod) || !Modifier.isFinal(mod)) {
                            continue;
                        }

                        // Look through the class for "summary" and "full help"
                        // methods.
                        Method summaryMethod = null;
                        Method docMethod = null;
                        final Method[] methods = servclass.getMethods();
                        for (int m = 0; m < methods.length; m++) {
                            if (DOC_METHOD_NAME.equals(methods[m].getName())) {
                                docMethod = methods[m];
                            } else if (SUMMARY_METHOD_NAME.equals(methods[m].getName())) {
                                summaryMethod = methods[m];
                            }
                            if (docMethod != null && summaryMethod != null) {
                                break;
                            }
                        }
                        if (summaryMethod != null) {
                            final Object[] params = new Object[0];
                            try {
                                String summary = (String) summaryMethod.invoke(servclass
                                        .newInstance(), params);
                                out.printf(SUMMARY_FORMAT, cmd, summary);
                                if (ah.hasLongFlag() && docMethod != null) {
                                    String doc = (String) docMethod.invoke(servclass.newInstance(),
                                            params);
                                    out.println(doc);
                                }
                            } catch (final Exception e) {
                                // It's only help, after all.
                            }
                        }
                    }
                } else {
                    String cmd = arglist.get(0);
                    StringBuilder sb = new StringBuilder();
                    sb.append(Http.ACTION_URL).append(SLASH).append(cmd);
                    sb.append(QUESTION_MARK).append("ARGS=-h"); //$NON-NLS-1$
                    if (ah.hasLongFlag()) {
                        sb.append(AMPERSAND).append("ARGS=-l"); //$NON-NLS-1$
                    }
                    if (ah.hasShortFlag()) {
                        sb.append(AMPERSAND).append("ARGS=-s"); //$NON-NLS-1$
                    }
                    sb.append(AMPERSAND).append("proj=").append(projectName); //$NON-NLS-1$
                    sb.append(AMPERSAND).append("rwd="); //$NON-NLS-1$
                    String newurl = sb.toString();
                    RequestDispatcher rd = req.getRequestDispatcher(newurl);
                    rd.forward(req, res);
                }
            } else {
                // Search the path for classes matching "Command*".
                final String uccmd = action.substring(0, 1).toUpperCase() + action.substring(1);

                // Look for binaries (*.class) first.
                final ClassGlob glob = new ClassGlob(uccmd, CLASS_GLOB);
                final String[] matches = clsdir.list(glob);

                if (matches == null || matches.length == 0) {
                    errmsg = ActionMessages.getString("Actions.2"); //$NON-NLS-1$
                } else if (matches.length > 1) {
                    // The supplied pattern was ambiguous - let it fail
                    // with an appropriate message.
                    classname = pkg + DOT + uccmd;
                    // The following causes complete matches such as "ls" to
                    // break.
                    // errmsg = "ambiguous action name";
                } else {
                    // Success.
                    final String match = matches[0];
                    classname = pkg + DOT + match.substring(0, match.indexOf('.'));
                }

                /*
                 * If we found exactly one class above, check it to see if it's
                 * a valid action. If so, we should end up with a reference to
                 * the required method.
                 */

                if (errmsg == null && classname != null) {
                    Class<?> action_class = Class.forName(classname);
                    /*
                     * There's a more direct way to find doGet but I haven't
                     * gone to the trouble. Looks like doGet is generally listed
                     * first anyway.
                     */
                    final Method[] methods = action_class.getMethods();
                    for (int i = 0; i < methods.length; i++) {
                        if (ACTION_METHOD_NAME.equals(methods[i].getName())) {
                            domethod = methods[i];
                            break;
                        }
                    }
                    if (domethod == null) {
                        errmsg = ActionMessages.getString("Actions.2"); //$NON-NLS-1$
                    }

                    if (action_class == null || domethod == null) {
                        errmsg = ActionMessages.getString("Actions.2"); //$NON-NLS-1$
                    } else if (project.isEmpty() && !PLUS.equals(projectName)
                            && !classname.contains(ABOUT_ACTION_NAME)
                            && !classname.contains(LS_PROJECTS_ACTION_NAME)
                            && !classname.contains(ADMIN_ACTION_NAME) && !ah.hasHelpFlag()) {
                        /*
                         * Guard against an attempt to post-mortem something
                         * that hasn't lived yet. As a special case, we let
                         * "lsprojects" through to avoid a catch-22.
                         */
                        errmsg = ActionMessages.getString("Redirector.1") + ' ' + projectName; //$NON-NLS-1$
                    } else {
                        if (!classname.contains(LS_PROJECTS_ACTION_NAME)) {
                            ah.registerIDOption();
                            ah.registerProjectOption();
                        }

                        final Object[] params = new Object[1];
                        params[0] = ah;

                        // All systems go. Invoke the method. If project is
                        // specified as "+" it means "for all projects do ...".
                        if (PLUS.equals(projectName)) {
                            for (Project project2 : dataService.getProjectList()) {
                                ah.setProject(project2);
                                domethod.invoke(action_class.newInstance(), params);
                            }
                        } else {
                            domethod.invoke(action_class.newInstance(), params);
                        }
                    }
                }
            }

            dataService.commitTransaction();

        } catch (ClassNotFoundException e) {
            logger.error(NULL_STRING, e);
            DataServiceHelper.rollbackTx(dataService);
            errmsg = ActionMessages.getString("Actions.2"); //$NON-NLS-1$
        } catch (Exception e) {
            logger.error(NULL_STRING, e);
            DataServiceHelper.rollbackTx(dataService);
            throw new ServletException(e.getCause());
        } finally {
            DataServiceHelper.close(dataService);
        }

        if (errmsg != null && ah != null) {
            ah.errorMessage(errmsg);
        }
    }
}
