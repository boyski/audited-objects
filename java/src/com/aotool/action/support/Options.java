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

package com.aotool.action.support;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.apache.log4j.Logger;

import com.aotool.action.ActionMessages;
import com.aotool.entity.Project;
import com.aotool.web.Http;
import com.martiansoftware.jsap.FlaggedOption;
import com.martiansoftware.jsap.JSAP;
import com.martiansoftware.jsap.JSAPResult;
import com.martiansoftware.jsap.Parameter;
import com.martiansoftware.jsap.Switch;
import com.martiansoftware.jsap.UnflaggedOption;
import com.martiansoftware.jsap.UnspecifiedParameterException;

/**
 * The Options class uses JSAP to parse flags passed to action commands.
 */
public class Options extends JSAP {

    private static final String PROJECT_OPTION = ActionMessages.getString("Flags.10"); //$NON-NLS-1$
    private static final String ABSOLUTE_FLAG = ActionMessages.getString("Flags.11"); //$NON-NLS-1$
    private static final String FORCE_FLAG = ActionMessages.getString("Flags.12"); //$NON-NLS-1$
    protected static final String HELP_FLAG = ActionMessages.getString("Flags.13"); //$NON-NLS-1$
    private static final String ID_OPTION = ActionMessages.getString("Flags.14"); //$NON-NLS-1$
    private static final String LONG_FLAG = ActionMessages.getString("Flags.15"); //$NON-NLS-1$
    private static final String MEMBERS_ONLY_FLAG = ActionMessages.getString("Flags.16"); //$NON-NLS-1$
    private static final String RECURSIVE_FLAG = ActionMessages.getString("Flags.7"); //$NON-NLS-1$
    private static final String SHORT_FLAG = ActionMessages.getString("Flags.8"); //$NON-NLS-1$
    private static final String VERBOSE_FLAG = ActionMessages.getString("Flags.9"); //$NON-NLS-1$
    private static final String EXTENDED_NAMES_FLAG = ActionMessages.getString("Flags.0"); //$NON-NLS-1$
    private static final String DATA_COMPARE_FLAG = ActionMessages.getString("Flags.20"); //$NON-NLS-1$

    private static final String ARGUMENTS = "ARG"; //$NON-NLS-1$

    private static final String RESERVED_OPTION_MSG = ActionMessages.getString("Options.0"); //$NON-NLS-1$

    private static final Logger logger = Logger.getLogger(Options.class);

    protected Project project;
    private final Map<String, String[]> reqParams;
    private final Map<String, Boolean> booleanOverrides = new HashMap<String, Boolean>();
    private String action;
    private JSAPResult parsed;

    private char clientPlatform;

    /**
     * Instantiates a new Options object.
     * 
     * @param reqParams
     *            the req params
     */
    public Options(String action, Project project, Map<String, String[]> reqParams) {
        super();

        this.action = action;
        this.project = project;
        this.reqParams = reqParams;
    }

    /**
     * Parses the command line for all registered options and flags.
     * 
     * @throws ActionException
     * @throws IOException
     */
    @SuppressWarnings("unchecked")
    public String parse() throws ActionException, IOException {
        String message = null;

        if (reqParams.containsKey(Http.CLIENT_PLATFORM_PARAM)) {
            String[] platstr = reqParams.get(Http.CLIENT_PLATFORM_PARAM);
            clientPlatform = platstr[0].charAt(0);
        } else {
            clientPlatform = 'u';
        }

        /*
         * Register a handler for trailing operands, typically file names. Must
         * be done just before parsing to ensure args show up after all options
         * in help msgs.
         */
        try {
            this.registerParameter(new UnflaggedOption(ARGUMENTS, JSAP.STRING_PARSER, null, false,
                    true));
        } catch (Exception e) {
            // Ignored for convenience
        }

        String[] args = reqParams.get("ARGS"); //$NON-NLS-1$
        if (args == null) {
            args = new String[0];
        }

        parsed = this.parse(args);

        if (!parsed.success()) {
            StringBuilder sb = new StringBuilder();
            for (Iterator<String> errs = parsed.getErrorMessageIterator(); errs.hasNext();) {
                sb.append("Error: ").append(errs.next()).append('\n'); //$NON-NLS-1$
            }
            throw new ActionException(sb.toString());
        } else if (hasHelpFlag() && parsed.getBoolean(HELP_FLAG)) {
            message = ActionMessages.getString("Options.11") //$NON-NLS-1$
                    + ' ' + this.getAction() + ' ' + this.getUsage() + ' ' + '\n'
                    + '\n'
                    + ActionMessages.getString("Options.12") //$NON-NLS-1$
                    + ':' + '\n' + this.getHelp() + '\n';
        }

        return message;
    }

    public Project getProject() {
        return project;
    }

    public void setProject(Project project) {
        this.project = project;
    }

    /**
     * Registers that the calling action is able to parse the -a/bsolute flag.
     */
    public void registerAbsoluteFlag() {
        this.addFlagOption('a', ABSOLUTE_FLAG, ActionMessages.getString("Options.2")); //$NON-NLS-1$
    }

    /**
     * Registers that the calling action is able to parse the -d flag.
     */
    public void registerDataCompareFlag() {
        this.addFlagOption('d', DATA_COMPARE_FLAG, ActionMessages.getString("Options.14")); //$NON-NLS-1$
    }

    /**
     * Registers that the calling action is able to parse the -f/orce flag.
     */
    public void registerForceFlag() {
        this.addFlagOption('f', FORCE_FLAG, ActionMessages.getString("Options.3")); //$NON-NLS-1$
    }

    /**
     * Registers that the calling action is able to parse the -l/ong flag.
     */
    public void registerLongFlag() {
        this.addFlagOption('l', LONG_FLAG, ActionMessages.getString("Options.4")); //$NON-NLS-1$
    }

    /**
     * Registers that the calling action is able to parse the -m/embers_only
     * flag.
     */
    public void registerMembersOnlyFlag() {
        this.addFlagOption('m', MEMBERS_ONLY_FLAG, ActionMessages.getString("Options.5")); //$NON-NLS-1$
    }

    /**
     * Registers that the calling action is able to parse the -r/ecursive flag.
     */
    public void registerRecursiveFlag() {
        this.addFlagOption('r', RECURSIVE_FLAG, ActionMessages.getString("Options.6")); //$NON-NLS-1$
    }

    /**
     * Registers that the calling action is able to parse the -s/hort flag.
     */
    public void registerShortFlag() {
        this.addFlagOption('s', SHORT_FLAG, ActionMessages.getString("Options.7")); //$NON-NLS-1$
    }

    /**
     * Registers that the calling action is able to parse the -v/erbose flag.
     */
    public void registerVerboseFlag() {
        this.addFlagOption('v', VERBOSE_FLAG, ActionMessages.getString("Options.8")); //$NON-NLS-1$
    }

    /**
     * Registers that the calling action is able to parse the -x/-extended-names
     * flag.
     */
    public void registerExtendedNamesFlag() {
        this.addFlagOption('x', EXTENDED_NAMES_FLAG, ActionMessages.getString("Options.13")); //$NON-NLS-1$
    }

    /**
     * Registers the set of flags which all path-formatting actions must handle.
     */
    public void registerStandardFlags() {
        this.registerAbsoluteFlag();
        this.registerDataCompareFlag();
        this.registerExtendedNamesFlag();
        this.registerLongFlag();
        this.registerMembersOnlyFlag();
        this.registerShortFlag();
    }

    /**
     * Registers that the calling action is able to parse the -i/d option.
     */
    public void registerIDOption() {
        this.addStringOption(false, ID_OPTION, null, false, 'i', true, ActionMessages
                .getString("Options.9")); //$NON-NLS-1$
    }

    /**
     * Registers that the calling action is able to parse the -p/roject option.
     */
    public void registerProjectOption() {
        this.addStringOption(true, PROJECT_OPTION, null, false, 'p', false, ActionMessages
                .getString("Options.10")); //$NON-NLS-1$
    }

    /**
     * Sets the help message.
     * 
     * @param id
     *            the id
     * @param msg
     *            the msg
     */
    public void setHelp(String id, String msg) {
        Parameter op = this.getByID(id);
        op.setHelp(msg);
    }

    /**
     * Gets the sub cmd.
     * 
     * @return the sub cmd
     */
    public String getAction() {
        return action;
    }

    private void addStringOption(boolean fallback, String id, String defaultval, boolean required,
            char shortname, boolean multiples, String help) {

        try {
            FlaggedOption op = new FlaggedOption(id, JSAP.STRING_PARSER, defaultval, required,
                    shortname, id);
            if (help != null) {
                op.setHelp(help);
            }
            op.setAllowMultipleDeclarations(multiples);
            this.registerParameter(op);
        } catch (Exception e) {
            logger.error("_addStringOption" //
                    + ", fallback = " + fallback //
                    + ", id = " + id //
                    + ", defaultval = " + defaultval //
                    + ", required = " + required //
                    + ", shortname = " + shortname //
                    + ", multiples = " + multiples //
                    + ", help = " + help//
            , e);
        }
    }

    /**
     * Registers the specified string-valued option as one accepted by the
     * calling action.
     * 
     * @param fallback
     *            the fallback
     * @param fullname
     *            the full name of the option
     * @param defaultval
     *            the default value
     * @param required
     *            true if this option is required
     * @param shortname
     *            the short name of the option
     * @param multiples
     *            true if it is legal to pass this option multiple times
     * @param help
     *            a help string which is added to the usage message
     */
    public void registerStringOption(boolean fallback, String fullname, String defaultval,
            boolean required, char shortname, boolean multiples, String help) {

        assert (shortname == JSAP.NO_SHORTFLAG || !Character.isLowerCase(shortname)) : RESERVED_OPTION_MSG;
        this.addStringOption(fallback, fullname, defaultval, required, shortname, multiples, help);
    }

    private void addFlagOption(char shortname, String id, String help) {
        try {
            Switch op = new Switch(id, shortname, id);
            if (help != null) {
                op.setHelp(help);
            }
            if (this.getByID(id) == null) {
                this.registerParameter(op);
            }
        } catch (Exception e) {
            logger.error("_addFlagOption" //
                    + ", shortname = " + shortname //
                    + ", id = " + id //
                    + ", help = " + help//
            , e);
        }
    }

    /**
     * Registers the specified flag as one accepted by the calling action.
     * 
     * @param shortname
     *            the short name of the flag
     * @param fullname
     *            the full name of the flag
     * @param help
     *            a help string which is added to the usage message
     */
    public void registerFlagOption(char shortname, String fullname, String help) {
        assert (shortname == JSAP.NO_SHORTFLAG || !Character.isLowerCase(shortname)) : RESERVED_OPTION_MSG;
        this.addFlagOption(shortname, fullname, help);
    }

    /**
     * Tests for the presence of the boolean flag named. This is used for
     * non-standard flags; each standard flag has its own dedicated method.
     * 
     * @param name
     *            the name of the flag
     * 
     * @return true if the specified flag is present
     */
    public boolean hasFlag(String name) {
        boolean result;
        if (booleanOverrides.containsKey(name)) {
            result = booleanOverrides.get(name);
        } else if (parsed == null) {
            result = false;
        } else {
            try {
                result = parsed.getBoolean(name);
            } catch (UnspecifiedParameterException e) {
                result = false;
            }
        }
        return result;
    }

    /**
     * Tests for the presence of the -absolute flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasAbsoluteFlag() {
        return this.hasFlag(ABSOLUTE_FLAG);
    }

    /**
     * Tests for the presence of the -data-compare flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasDataCompareFlag() {
        return this.hasFlag(DATA_COMPARE_FLAG);
    }

    /**
     * Tests for the presence of the -force flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasForceFlag() {
        return this.hasFlag(FORCE_FLAG);
    }

    /**
     * Tests for the presence of the -help flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasHelpFlag() {
        return this.hasFlag(HELP_FLAG);
    }

    /**
     * Tests for the presence of the -long flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasLongFlag() {
        return this.hasFlag(LONG_FLAG);
    }

    /**
     * Tests for the presence of the -members only flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasMembersOnlyFlag() {
        return this.hasFlag(MEMBERS_ONLY_FLAG);
    }

    /**
     * Tests for the presence of the -recursive flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasRecursiveFlag() {
        return this.hasFlag(RECURSIVE_FLAG);
    }

    /**
     * Tests for the presence of the -short flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasShortFlag() {
        return this.hasFlag(SHORT_FLAG);
    }

    /**
     * Tests for the presence of the -verbose flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasVerboseFlag() {
        return this.hasFlag(VERBOSE_FLAG);
    }

    /**
     * Tests for the presence of the -extended-name flag.
     * 
     * @return true if the flag is set
     */
    public boolean hasExtendedNamesFlag() {
        return this.hasFlag(EXTENDED_NAMES_FLAG);
    }

    public void setExtendedNamesFlag(boolean state) {
        booleanOverrides.put(EXTENDED_NAMES_FLAG, state);
    }

    public void setHelpFlag(boolean state) {
        booleanOverrides.put(HELP_FLAG, state);
    }

    private String getStringOption(String name) {
        String result = parsed.getString(name);
        if (result == null && reqParams.containsKey(name)) {
            result = reqParams.get(name)[0];
        }
        return result;
    }

    private String[] getStringOptions(String name) {
        String[] results = parsed.getStringArray(name);
        return results;
    }

    /**
     * Returns the value of the -id option.
     * 
     * @return a PTX id string, or null
     */
    public String getIDOption() {
        String id = this.getStringOption(ID_OPTION);
        if ("+".equals(id)) {
            id = project.getMostRecentPtx().getIdString();
        }
        return id;
    }

    public String[] getIDOptions() {
        String[] ids = this.getStringOptions(ID_OPTION);
        return ids;
    }

    /**
     * Returns the value of the -id option or the latest PTX id.
     * 
     * @return a PTX id string
     */
    public String getIDOptionOrMostRecent() {
        String id = this.getIDOption();
        if (id == null) {
            id = project.getMostRecentPtx().getIdString();
        }
        return id;
    }

    /**
     * Tests for the presence of the -project option.
     * 
     * @return the value of the option
     */
    public String getProjectNameOption() {
        return this.getStringOption(PROJECT_OPTION);
    }

    /**
     * Gets all action arguments in List form.
     * 
     * @return a List of the arguments
     */
    public ArrayList<String> getArguments() {
        String[] args = parsed.getStringArray(ARGUMENTS);
        // For some strange reason an empty arg list shows up
        // as an array of one null string. Workaround here.
        if (args.length == 1 && args[0].length() == 0) {
            args = new String[0];
        }
        ArrayList<String> arglist = new ArrayList<String>(Arrays.asList(args));
        return arglist;
    }

    /**
     * Tests whether the current client connection comes from a Windows system.
     * This may be used to adapt the presentation of filenames etc.
     * 
     * @return true, if client runs Windows
     */
    public boolean clientIsWindows() {
        return clientPlatform == 'w';
    }

    /**
     * Tests whether the current client connection comes from a Cygwin
     * environment. This may be used to adapt the presentation of filenames etc.
     * 
     * @return true, if client runs Cygwin
     */
    public boolean clientIsCygwin() {
        return clientPlatform == 'c';
    }
}
