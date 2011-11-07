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
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

import javax.servlet.ServletContext;

import org.apache.log4j.Logger;

public class AppProperties {

    private static final Logger logger = Logger.getLogger(AppProperties.class);

    private interface PropNames {

        String APPLICATION_VERSION = "APPLICATION_VERSION"; //$NON-NLS-1$

        String CONTAINER_ROOT_DIR = "AO.Container.Root"; //$NON-NLS-1$
        String SERVER_PROPERTIES_FILE = "AO.Server.Properties"; //$NON-NLS-1$
        String NEEDS_SYNTHETIC_DATABASE_LOCKS = "AO.Needs.Synthetic.Database.Locks";

        interface JpaPrefix {
            String Prefix1 = "javax.persistence.";
            String Prefix2 = "eclipselink.";
        }
    }

    private static final String FILE_CONTAINER_DIR = "AO_FILE_CONTAINERS"; //$NON-NLS-1$

    private static Map<String, Object> props;

    public static void initialize(ServletContext context) throws FileNotFoundException, IOException {

        props = loadContextInitParams(context);

        if (getProperty(PropNames.CONTAINER_ROOT_DIR) == null) {
            String containerRoot = context.getRealPath("/"); //$NON-NLS-1$
            props.put(PropNames.CONTAINER_ROOT_DIR, containerRoot);
        }
    }

    @SuppressWarnings({ "unchecked", "rawtypes" })
    private static Map<String, Object> loadContextInitParams(ServletContext context)
            throws FileNotFoundException, IOException {

        Properties props = new Properties();

        // Load servlet context properties.
        for (Enumeration<String> en = context.getInitParameterNames(); en.hasMoreElements();) {
            String key = en.nextElement();
            Object value = context.getInitParameter(key);
            props.put(key.toLowerCase(), value);
            if (logger.isDebugEnabled()) {
                logger.debug("Using init parameter" // $NON-NLS-1$
                        + ' ' + key + '=' + value);
            }
        }

        // Populate override properties from a file if specified.
        String propsFileName = context.getInitParameter(PropNames.SERVER_PROPERTIES_FILE);
        if (propsFileName == null) {
            propsFileName = System.getProperty(PropNames.SERVER_PROPERTIES_FILE);
            if (propsFileName == null) {
                propsFileName = System.getProperty(PropNames.SERVER_PROPERTIES_FILE.toLowerCase());
            }
        }

        if (propsFileName != null) {
            File propsFile = new File(propsFileName);
            logger.info("Getting property overrides from " + propsFile.getAbsolutePath());
            InputStream is = new FileInputStream(propsFile);
            if (is != null) {
                Properties tprops = new Properties();
                tprops.load(is);
                is.close();
                for (Object key : tprops.keySet()) {
                    String name = (String) key;
                    props.put(name.toLowerCase(), tprops.get(key));
                }
            }
        }

        logger.info("JDBC URL is " + props.getProperty("javax.persistence.jdbc.url"));

        return new HashMap(props);
    }

    /**
     * Gets a project or site preference value.
     * 
     * @param name
     *            the preference name
     * 
     * @return the stored object
     */
    @SuppressWarnings("unchecked")
    private static <T> T getProperty(String name) {
        Object value = null;
        if (props != null) {
            String lcName = name.toLowerCase();
            synchronized (props) {
                value = props.get(lcName);
            }
            if (value == null) {
                value = System.getProperty(lcName);
            }
        }
        return (T) value;
    }

    /**
     * Sets a project or site preference. These are separate from user
     * properties, which may be set and overwritten from the client side at any
     * time. Site preferences are for choices which apply to all users and must
     * remain stable.
     * 
     * @param name
     *            the name under which the object will be saved
     * @param value
     *            the object to be saved
     */
    public static void setProperty(String name, Object value) {
        synchronized (props) {
            String lcName = name.toLowerCase();
            props.put(lcName, value);
        }
    }

    private static boolean getBooleanProperty(String name) {
        String value = (String) getProperty(name);
        return value != null && value.equalsIgnoreCase("true");
    }

    /**
     * Gets the application version.
     * 
     * @return the application version
     */
    public static String getApplicationVersion() {
        return getProperty(PropNames.APPLICATION_VERSION);
    }

    /**
     * Gets the path of the base directory under which file data containers (and
     * nothing else) are stored.
     * 
     * @return the container base dir
     */
    public static File getContainerRootDir() {
        String rootDir = (String) getProperty(PropNames.CONTAINER_ROOT_DIR);
        return new File(rootDir, FILE_CONTAINER_DIR);
    }

    public static boolean needsSyntheticDatabaseLocks() {
        return getBooleanProperty(PropNames.NEEDS_SYNTHETIC_DATABASE_LOCKS);
    }

    private static Map<String, String> jpaProps;
    private static boolean jpaPropsInitialized = false;

    public static Map<String, String> getJpaProps() {
        if (!jpaPropsInitialized) {
            synchronized (props) {
                Map<String, String> jpaPropsTemp = new HashMap<String, String>();
                for (Map.Entry<String, Object> e : props.entrySet()) {
                    String key = e.getKey();
                    if (key.startsWith(PropNames.JpaPrefix.Prefix1)
                            || key.startsWith(PropNames.JpaPrefix.Prefix2)) {
                        jpaPropsTemp.put(key, String.valueOf(e.getValue()));
                    }
                }
                jpaProps = jpaPropsTemp;
                jpaPropsInitialized = true;
            }
        }
        return jpaProps;
    }
}
