package com.aotool.util;

import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import javax.persistence.Query;

import com.aotool.Constants;
import com.aotool.action.support.ActionHelper;
import com.aotool.entity.DataService;
import com.aotool.entity.Moment;
import com.aotool.entity.PathAction;
import com.aotool.entity.PathName;
import com.aotool.entity.PathState;
import com.aotool.entity.Ptx;

public final class PathFormat {

    /** The "extended naming string". */
    private static final String XNS = "@@"; //$NON-NLS-1$

    /** An array of strings used in the common Unix "ls -l" format. */
    private static final String[] modeArray = { "---", "--x", "-w-", "-wx", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
            "r--", "r-x", "rw-", "rwx", }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$

    /**
     * A comparator for ordering PathAction objects by last-modified time.
     */
    private static final Comparator<PathAction> ORDER_PATHACTIONS_LATEST_LAST = new Comparator<PathAction>() {
        public int compare(PathAction pa1, PathAction pa2) {
            return pa1.compareMtime(pa2);
        }
    };

    private static String format(String pathString, String extension) {
        StringBuilder sb = new StringBuilder();
        sb.append(pathString);
        if (extension != null && extension.length() > 0) {
            sb.append(PathFormat.XNS);
            sb.append(extension);
        }
        return sb.toString();
    }

    public static String format(Ptx ptx, String pathString) {
        return format(pathString, ptx.getName());
    }

    public static String format(PathAction pa) {
        String result;
        String stateName = pa.getPathState().getStateName();
        if (stateName == null) {
            Ptx ptx = pa.getPtx();
            result = format(ptx, pa.getPathString());
        } else {
            result = format(pa.getPathString(), stateName);
        }
        return result;
    }

    private static String format(String str, PathState ps) {
        String extension;
        if (ps.hasDataCode()) {
            extension = ps.getDataCode();
        } else {
            extension = ps.getTimeStamp().toStandardString();
        }
        return format(str, extension);
    }

    /**
     * Returns a string representing the path state in the POSIX standard
     * "ls -l" format.
     * 
     * @param base
     *            the base directory, or null if not needed
     * @param dos
     *            indicates whether to use \ as the separator
     * @param logname
     *            the string to show in the login name field
     * @param groupname
     *            the string to show in the group name field
     * 
     * @return the string
     */
    private static String getLsFormatString(PathState ps, String base, boolean dos, Ptx ptx) {
        int umode = (ps.getMode() >> 6) & 07;
        int gmode = (ps.getMode() >> 3) & 07;
        int omode = (ps.getMode() >> 0) & 07;
        String dc = ps.getDataCode();
        String ds = (dc == null || dc.length() == 0) ? "-" : dc;
        int dsLen = ptx.getDataCodeWidth();
        Moment mtime = ps.getTimeStamp();
        String user = ptx.getLogName();
        String group = ptx.getGroupName();
        // Java does not seem to support the '*' width feature ...
        String fmt = "-%s%s%s  %-" + Integer.valueOf(dsLen) + "s %-8s %-8s %7d %s %s"; //$NON-NLS-1$ $NON-NLS-2$
        String ls = String.format(fmt,
                modeArray[umode], modeArray[gmode], modeArray[omode], ds, user, group,
                ps.getSize(), mtime.toLsStyleString(), getNativePathString(ps.getPathName(), base,
                        dos));
        return ls;
    }

    /**
     * Returns a string representing the path state in a "stat" format, showing
     * only the metadata which express critical differences in content (size,
     * data code).
     * 
     * @param base
     *            the base directory, or null if not needed
     * @param dos
     *            indicates whether to use \ as the separator
     * 
     * @return the string
     */
    private static String getDataCodeFormatString(PathState ps, String base, ActionHelper ah) {
        final String dc = ps.getDataCode();
        final String name, value;
        if (dc == null || dc.length() == 0) {
            name = "mtime";
            value = ps.getTimeStamp().toStandardString();
        } else {
            name = "dcode";
            value = dc;
        }
        final String path = getNativePathString(ps.getPathName(), base, ah.clientIsWindows());
        final String ls;
        if (ah.hasShortFlag()) {
            ls = String.format("%-14s %s", value, path); //$NON-NLS-1$
        } else if (ah.hasLongFlag()) {
            ls = String
                    .format(
                            "%s=%-14s size=%-10d moment=%s %s", name, value, ps.getSize(), ps.getTimeStamp().toStandardString(), path); //$NON-NLS-1$
        } else {
            ls = String.format("%-14s %-10d %s", value, ps.getSize(), path); //$NON-NLS-1$
        }
        return ls;
    }

    /**
     * Gets the path string in the native form for the requesting client.
     * 
     * @param base
     *            the base dir from which to create absolute paths
     * @param windowsStyle
     *            if true, use backslash as the path separator
     * 
     * @return the native path string
     */
    public static String getNativePathString(PathName pn, String base, boolean windowsStyle) {
        String nativePath;
        if (base != null && pn.isMember()) {
            nativePath = base + '/' + pn.getPathString();
        } else {
            nativePath = pn.getPathString();
        }
        if (windowsStyle) {
            nativePath = nativePath.replace('/', '\\');
        }
        return nativePath;
    }

    /**
     * Gets the path string in the native form for the requesting client.
     * 
     * @param windowsStyle
     *            if true, use backslash as the path separator
     * 
     * @return the native path string
     */
    private static String getNativePathString(PathName pn, boolean windowsStyle) {
        return getNativePathString(pn, null, windowsStyle);
    }

    public static String getNativePathString(PathAction pa, String base, boolean windowsStyle) {
        return getNativePathString(pa.getPathState().getPathName(), windowsStyle);
    }

    public static String toPrintableString(Ptx ptx, PathState ps, ActionHelper ah) {
        String str;
        String base = ah.hasAbsoluteFlag() ? ptx.getBaseDirectory() : null;
        if (ah.hasDataCompareFlag()) {
            str = PathFormat.getDataCodeFormatString(ps, base, ah);
        } else if (ah.hasLongFlag()) {
            str = PathFormat.getLsFormatString(ps, base, ah.clientIsWindows(), ptx);
        } else {
            str = PathFormat.getNativePathString(ps.getPathName(), base, ah.clientIsWindows());
        }
        if (ah.hasExtendedNamesFlag()) {
            if (ah.hasShortFlag()) {
                str = PathFormat.format(str, ps);
            } else {
                str = PathFormat.format(ptx, str);
            }
        }
        return str;
    }

    public static String toPrintableString(PathAction pa, ActionHelper ah) {
        return toPrintableString(pa.getPtx(), pa.getPathState(), ah);
    }

    /**
     * Derives a list of all PAs using the given pathname in this PTX, and
     * returns the list sorted by PA time.
     * 
     * @param pathString
     *            the path to search within the current PTX
     * @return a list of PathActions, possibly empty
     */
    @SuppressWarnings("unchecked")
    public static List<PathAction> getOrderedPathActionListInPtx(String pathString,
            DataService dataService, Ptx ptx) {
        Query query = dataService.createQuery("" //
                + " select pa" //
                + "   from PathAction pa" //
                + "   where pa.commandAction.ptx.id = :ptxId" //
                + "     and pa.pathState.pathName.pName = :pathString" //
        );
        query.setParameter("ptxId", ptx.getId());
        query.setParameter("pathString", pathString);
        List<PathAction> paList = query.getResultList();
        Collections.sort(paList, ORDER_PATHACTIONS_LATEST_LAST);
        return paList;
    }

    /**
     * Derives a list of all PAs using the given path in any PTX, and returns
     * the list sorted by PA time.
     * 
     * @param pathString
     *            the path to search for
     * @return a list of PathActions, possibly empty
     */
    @SuppressWarnings("unchecked")
    public static List<PathAction> getOrderedPathActionListByPath(String pathString,
            DataService dataService) {
        Query query = dataService.createQuery("" //
                + " select pa" //
                + "   from PathAction pa" //
                + "   where pa.pathState.pathName.pName = :pathString" //
        );
        query.setParameter("pathString", pathString);
        List<PathAction> paList = query.getResultList();
        Collections.sort(paList, ORDER_PATHACTIONS_LATEST_LAST);
        return paList;
    }

    /**
     * Returns the most recent PathAction using the given path within this PTX.
     * If the path was not used in this PTX, or the last action was to delete
     * it, null is returned.
     * 
     * @param pathString
     *            the path to search within the current PTX
     * @return the most recent PathAction, or null
     */
    private static PathAction getFinalPathActionInPtx(String pathString, DataService dataService,
            Ptx ptx) {
        List<PathAction> paList = getOrderedPathActionListInPtx(pathString, dataService, ptx);
        PathAction pathAction = null;
        if (!paList.isEmpty()) {
            pathAction = paList.get(0);
            if (pathAction.isUnlink()) {
                pathAction = null;
            }
        }
        return pathAction;
    }

    @SuppressWarnings("unchecked")
    public static PathAction findByExtendedName(String name, DataService dataService) {
        PathAction pathAction = null;
        String pathString;
        String extension;
        int xns = name.indexOf(PathFormat.XNS);
        if (xns == -1) {
            Query query = dataService.createQuery("" //
                    + " select ptx from Ptx ptx" //
            );
            List<Ptx> ptxList = query.getResultList();
            for (int i = ptxList.size() - 1; i >= 0; i--) {
                Ptx ptx = ptxList.get(i);
                pathAction = getFinalPathActionInPtx(name, dataService, ptx);
                if (pathAction != null) {
                    break;
                }
            }
        } else {
            pathString = name.substring(0, xns);
            extension = name.substring(xns + PathFormat.XNS.length());
            if (extension.length() == 14) {
                Ptx ptx = Ptx.find(dataService, extension);
                pathAction = PathFormat.getFinalPathActionInPtx(pathString, dataService, ptx);
            } else {
                int dot = extension.indexOf('.');
                List<PathAction> paList = PathFormat.getOrderedPathActionListByPath(pathString,
                        dataService);
                for (PathAction pa : paList) {
                    PathState pathState = pa.getPathState();
                    if (dot == -1) {
                        if (pathState.hasDataCode()) {
                            long dc1 = Long.parseLong(pathState.getDataCode(), Constants.CSV_RADIX);
                            long dc2 = Long.parseLong(extension, Constants.CSV_RADIX);
                            if (dc1 == dc2) {
                                pathAction = pa;
                                break;
                            }
                        }
                    } else {
                        if (pathState.getTimeStamp().toStandardString().equals(extension)) {
                            pathAction = pa;
                            break;
                        }
                    }
                }
            }
        }
        return pathAction;
    }

}
