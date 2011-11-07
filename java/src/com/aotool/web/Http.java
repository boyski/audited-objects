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

import org.apache.log4j.Logger;

/**
 * The Http class is a container for constants used in HTTP communication, in
 * particular parameter names. It also provides a few static methods used for
 * HTTP communication.
 * 
 * <b>WARNING: most if not all of these constants are replicated on the client
 * side! Also, they should not be externalized/localized.</b>
 * 
 */
public final class Http {

    private static final Logger logger = Logger.getLogger(Http.class);

    public static final String ACTION_URL = "/action";

    /**
     * The preferred buffer size for reading and writing binary data. Current
     * idea is to align this with CURL_MAX_WRITE_SIZE (client side buffer). No
     * idea whether this makes any sense.
     */
    public static final int BUFFER_SIZE = 1024 * 32;

    /** An HTTP parameter used to request aggressive behavior (temporary). */
    public static final String AGGRESSIVE_PARAM = "aggressive";
    /** An HTTP parameter used to communicate the project name. */
    public static final String BASE_DIR_PARAM = "base_dir";
    /** An HTTP parameter indicating what type of OS the client runs. */
    public static final String CLIENT_PLATFORM_PARAM = "client_platform";
    /** An HTTP parameter used to communicate the PTX start time. */
    public static final String CLIENT_START_TIME_PARAM = "start_time";
    /** An HTTP parameter used to communicate the client version. */
    public static final String CLIENT_VERSION_PARAM = "clientver";
    /** An HTTP parameter used to communicate the user's group name. */
    public static final String GROUP_NAME_PARAM = "group";
    /** An HTTP parameter used to communicate the client host name. */
    public static final String HOST_NAME_PARAM = "hostname";
    /** An HTTP parameter used to communicate a label. */
    public static final String LABEL_PARAM = "label";
    /** An HTTP parameter used to override the default log4j logging level */
    public static final String LOG_LEVEL_PARAM = "loglevel";
    /** An HTTP boolean used to communicate that the body is a log file. */
    public static final String LOGFILE_PARAM = "logfile";
    /** An HTTP parameter used to communicate a user name. */
    public static final String LOGIN_NAME_PARAM = "logname";
    /** An HTTP parameter used to communicate the machine type. */
    public static final String MACHINE_TYPE_PARAM = "machine";
    /** An HTTP parameter used to communicate a generic name. */
    public static final String NAME_PARAM = "name";
    /** An HTTP parameter used to communicate the client host "uname -r" value. */
    public static final String OS_RELEASE_PARAM = "osrelease";
    /** An HTTP parameter used to communicate the project name. */
    public static final String PROJECT_NAME_PARAM = "project_name";
    /**
     * An HTTP parameter used to communicate a CSV string representing a path
     * state.
     */
    public static final String PS_CSV_PARAM = "pathstate";
    /* An HTTP parameter used to communicate a path state nickname. */
    public static final String PS_NAME_PARAM = "name";
    /** An HTTP parameter used to communicate the PTX strategy. */
    public static final String PTX_STRATEGY_PARAM = "ptx_strategy";
    /** An HTTP parameter used to communicate a preference. */
    public static final String READ_ONLY_PARAM = "readonly";
    /** An HTTP parameter used to communicate the roadmap name. */
    // public static final String ROADMAP_PARAM = "roadmap";
    /** An HTTP parameter used to communicate the RWD at time of request. */
    public static final String RWD_PARAM = "rwd";
    /** An HTTP parameter used to communicate a requested timeout, in seconds. */
    public static final String SESSION_TIMEOUT_SECS_PARAM = "session_timeout_secs";
    /** An HTTP parameter used to communicate a shopping preference. */
    public static final String SHOP_MEMBERS_ONLY_PARAM = "shop_members_only";
    /** An HTTP parameter used to communicate the name of the client host OS. */
    public static final String SYSTEM_NAME_PARAM = "sysname";
    /** An HTTP parameter indicating a bandwidth preference. */
    public static final String UNCOMPRESSED_TRANSFERS_PARAM = "uncompressed_transfers";
    /**
     * A property which is known on both client and server.
     */
    public static final String SESSION_ID_PROPERTY = "SESSIONID";

    /**
     * A magic header which, when encountered by the client, indicates a client
     * property which it needs to set.
     */
    public static final String X_SET_PROPERTY_HEADER = "X-Set-Property";

    /**
     * A custom header for communicating a return code from server to client.
     */
    public static final String X_SERVER_STATUS_HEADER = "X-Server-Status";

    /**
     * A custom header for communicating a return code from client to server.
     */
    public static final String X_CLIENT_STATUS_HEADER = "X-Client-Status";

    /**
     * A custom header for communicating a recycle count from client to server.
     */
    public static final String X_RECYCLED_COUNT_HEADER = "X-Recycled-Count";

    /**
     * A custom header which indicates that the body is in gzip format.
     */
    public static final String X_GZIPPED_HEADER = "X-GZIPPED";

    /**
     * A custom header which indicates that the body is a log file.
     */
    public static final String X_LOGFILE_HEADER = "X-LOGFILE";

    /**
     * A custom header whose value is a stringified PathState object.
     */
    public static final String X_PATHSTATE_HEADER = "X-PATHSTATE";

    /*
     * A standard HTTP header.
     */
    public static final String CONTENT_DISPOSITION_HEADER = "Content-Disposition";

    /*
     * A standard HTTP header.
     */
    public static final String CONTENT_ENCODING_HEADER = "Content-Encoding";

    /*
     * A standard MIME type.
     */
    public static final String APPLICATION_OCTET_STREAM = "application/octet-stream";

    /*
     * A standard MIME type.
     */
    public static final String TEXT_PLAIN = "text/plain; charset=UTF-8";

    /*
     * A standard encoding.
     */
    public static final String GZIP_ENCODING = "gzip";

    /**
     * A magic string which, when sent in an HTTP body, indicates that the
     * current line is an error message to be printed to stderr, and which
     * should cause the action to give a return code indicating error.
     */
    private static final String ERROR = "<<-ERROR->>: ";

    /**
     * A magic string which, when sent in an HTTP body, indicates that the
     * current line is a message to be printed to stderr but which has no effect
     * on the return code.
     */
    private static final String WARNING = "<<-WARNING->>: ";

    /**
     * A magic string which, when sent in an HTTP body, indicates an
     * informational message which should be printed to stdout.
     */
    private static final String NOTE = "<<-NOTE->>: ";

    /**
     * Returns an error message in a standard format.
     * 
     * @param msg
     *            the error summary
     * 
     * @return the complete error message
     */
    public static final String errorMessage(String msg) {
        logger.error(msg);
        String errmsg = ERROR + msg;
        if (!errmsg.endsWith("\n")) {
            errmsg += '\n';
        }
        return errmsg;
    }

    /**
     * Returns a warning message in a standard format.
     * 
     * @param msg
     *            the warning summary
     * 
     * @return the complete warning message
     */
    public static final String warningMessage(String msg) {
        logger.warn(msg);
        return WARNING + msg + "\n";
    }

    /**
     * Returns an informational message in a standard format.
     * 
     * @param msg
     *            the informational message
     * @param addendum
     *            an additional string for the message
     * 
     * @return a complete informational message
     */
    public static final String noteMessage(String msg, String addendum) {
        return NOTE + msg + ' ' + addendum + '\n';
    }

    /**
     * Returns an informational message in a standard format.
     * 
     * @param msg
     *            the informational message
     * 
     * @return a complete informational message
     */
    public static final String noteMessage(String msg) {
        return NOTE + msg + "\n";
    }
}
