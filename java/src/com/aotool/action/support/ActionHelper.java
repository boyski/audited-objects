package com.aotool.action.support;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Enumeration;
import java.util.List;
import java.util.Map;

import javax.servlet.ServletContext;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import com.aotool.action.ActionMessages;
import com.aotool.entity.DataService;
import com.aotool.entity.PathAction;
import com.aotool.entity.Project;
import com.aotool.entity.Ptx;
import com.aotool.entity.PtxBuilder;
import com.aotool.util.PathFormat;
import com.aotool.web.Http;
import com.aotool.web.servlet.RoadMap;

public class ActionHelper extends Options {
    private HttpServletRequest request;
    private HttpServletResponse response;
    private ServletContext context;
    private DataService dataService;

    public ActionHelper(String action, HttpServletRequest request, HttpServletResponse response,
            ServletContext context, Project project, DataService dataService) throws IOException {
        super(action, project, request.getParameterMap());
        this.request = request;
        this.response = response;
        this.context = context;
        this.dataService = dataService;

        // We may need to know about the help flag pre-parse ...
        Map<String, String[]> params = request.getParameterMap();
        String[] args = params.get("ARGS");
        if (args != null) {
            for (String arg : args) {
                if (arg.startsWith("-h")) {
                    this.setHelpFlag(true);
                    break;
                }
            }
        }
        // ... but we still want to be able to parse it.
        this.registerFlagOption('h', HELP_FLAG, ActionMessages.getString("Options.1")); //$NON-NLS-1$
    }

    public void errorMessage(String message) throws IOException {
        getWriter().print(Http.errorMessage(getAction() + ": " + message));
    }

    public void errorMessage(String message, String arg) throws IOException {
        String msg_plus_arg = message;
        if (arg != null) {
            msg_plus_arg += ": '" + arg + "'";
        }
        errorMessage(msg_plus_arg);
    }

    public boolean parseOptions() throws IOException {
        boolean success = false;
        try {
            String message = super.parse();
            if (message != null) {
                getWriter().print(Http.noteMessage(message));
            } else {
                success = true;
            }
        } catch (ActionException e) {
            errorMessage(e.getMessage());
        }
        return success;
    }

    public PrintWriter getWriter() throws IOException {
        return response.getWriter();
    }

    public void print(String str) throws IOException {
        getWriter().print(str);
    }

    public void print(char ch) throws IOException {
        getWriter().print(ch);
    }

    public void println() throws IOException {
        getWriter().println();
    }

    public void println(String line) throws IOException {
        getWriter().println(line);
    }

    public void printf(String format, Object... args) throws IOException {
        getWriter().printf(format, args);
    }

    public List<Project> getProjectList() {
        return dataService.getProjectList();
    }

    public Collection<Ptx> getPtxList() {
        return project.getPtxList();
    }

    public Ptx getPtx(String id) {
        Ptx ptx = Ptx.find(dataService, id);
        return ptx;
    }

    public Ptx getPtx() {
        return getPtx(getIDOptionOrMostRecent());
    }

    public List<Ptx> getTwoPtxes() {
        List<Ptx> two = new ArrayList<Ptx>();
        for (String id : this.getIDOptions()) {
            Ptx ptx = Ptx.find(dataService, id);
            two.add(ptx);
            if (two.size() == 2) {
                break;
            }
        }
        if (two.size() < 2) {
            List<Ptx> fullList = project.getPtxList();
            if (two.size() == 0 && fullList.size() > 1) {
                two.add(fullList.get(fullList.size() - 2));
            }
            if (fullList.size() > 0) {
                two.add(fullList.get(fullList.size() - 1));
            }
        }
        return two;
    }

    public PtxBuilder getPtxBuilder(Ptx ptx) {
        return new PtxBuilder(dataService, ptx);
    }

    public DataService getDataService() {
        return dataService;
    }

    public String getParameter(String name) {
        return request.getParameter(name);
    }

    public List<String> getHeaders(String name) {
        List<String> list = new ArrayList<String>();
        for (Enumeration<String> e = request.getHeaders(name); e.hasMoreElements();) {
            list.add(e.nextElement());
        }
        return list;
    }

    public String getRealPath(String path) {
        return context.getRealPath(path);
    }
    
    public String getRelativeWorkingDirectory() {
        return getParameter(Http.RWD_PARAM);
    }

    public List<PathAction> getOrderedPathActionListByPath(String pathString) {
        return PathFormat.getOrderedPathActionListByPath(pathString, dataService);
    }

    public void noSuchPtxError() throws IOException {
        errorMessage(ActionMessages.getString("Actions.1"), getIDOption()); //$NON-NLS-1$
    }

    public void scrubRoadMap() {
        RoadMap.scrub(context);
    }

}
