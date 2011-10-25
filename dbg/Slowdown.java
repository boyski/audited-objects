import java.text.SimpleDateFormat;
import java.util.*;
import java.sql.*;

class Slowdown {
    private static final String SQL_INTEGRITY_CONSTRAINT_VIOLATION = "23000";

    private static void createDB(Connection conn)
        throws SQLException {

        Statement stmt = conn.createStatement();

        try {
            stmt.execute(
                "CREATE TABLE Builds ("
                    + "ptxid BIGINT NOT NULL, "
                    + "PRIMARY KEY (ptxid))");

            stmt.execute(
                "CREATE TABLE Scripts ("
                    + "s_hash CHAR(10) NOT NULL, "
                    + "script VARCHAR(32) NOT NULL, "
                    + "PRIMARY KEY (s_hash))");

            stmt.execute(
                "CREATE TABLE Script_Moments ("
                    + "s_hash CHAR(10) NOT NULL, "
                    + "ptxid BIGINT NOT NULL, "
                    + "timedelta BIGINT, "
                    + "rwd VARCHAR(1024), "
                    + "umask INT, "
                    + "complete BIT NOT NULL, "
                    + "CONSTRAINT AWD UNIQUE (s_hash, ptxid), "
                    + "FOREIGN KEY (s_hash) "
                    + "REFERENCES Scripts (s_hash), "
                    + "FOREIGN KEY (ptxid) "
                    + "REFERENCES Builds (ptxid))");

            stmt.execute(
                "CREATE TABLE Paths ("
                    + "p_hash CHAR(10) NOT NULL, "
                    + "path VARCHAR(1024) NOT NULL, "
                    + "s_hash CHAR(10), "
                    + "PRIMARY KEY (p_hash),"
                    + "FOREIGN KEY (s_hash) "
                    + "REFERENCES Scripts (s_hash))");

            stmt.execute(
                "CREATE TABLE Path_Moments ("
                    + "p_hash CHAR(10) NOT NULL, "
                    + "s_hash CHAR(10) NOT NULL, "
                    + "ptxid BIGINT NOT NULL, "
                    + "istarget BIT NOT NULL, "
                    + "PRIMARY KEY (p_hash,s_hash,ptxid), "
                    + "FOREIGN KEY (p_hash) "
                    + "REFERENCES Paths (p_hash), "
                    + "FOREIGN KEY (s_hash) "
                    + "REFERENCES Scripts (s_hash), "
                    + "FOREIGN KEY (ptxid) "
                    + "REFERENCES Builds (ptxid))");

        }
        catch (SQLException e) {
            throw e;
        }
        finally {
            stmt.close();
        }
    }

    private static void addPTX(Connection conn, String ptxid)
        throws SQLException {

        String px =
            "INSERT INTO Builds (ptxid)"
                + " VALUES (?)";
	//System.out.println(px);
        PreparedStatement ps_px = conn.prepareStatement(px);
	ps_px.setString(1, ptxid);
	ps_px.executeUpdate();
	ps_px.close();
    }

    private static void addPaths(Connection conn, String scr, String sh, String ptxid, ArrayList al)
        throws SQLException {

        String pt =
            "INSERT INTO Paths (p_hash, path, s_hash)"
                + " VALUES (?, ?, ?)";
        PreparedStatement ps_pt = conn.prepareStatement(pt);

        String pmt =
            "INSERT INTO Path_Moments"
                + " (p_hash, s_hash, ptxid, istarget)"
                + " VALUES (?, ?, ?, ?)";
        PreparedStatement ps_pmt = conn.prepareStatement(pmt);

	//System.out.println(pt);
	//System.out.println(pmt);

        Iterator iter = al.iterator();
        while (iter.hasNext()) {
	    String pth = (String) iter.next();
	    String ph = new Long(Math.abs(pth.hashCode())).toString();

            ps_pt.clearParameters();
            ps_pt.setString(1, ph);
            ps_pt.setString(2, pth);
	    ps_pt.setString(3, sh);
            try {
                ps_pt.executeUpdate();
            }
            catch (SQLException e) {
                if (!e
                    .getSQLState()
                    .equals(SQL_INTEGRITY_CONSTRAINT_VIOLATION))
                    throw e;
            }

            ps_pmt.clearParameters();
            ps_pmt.setString(1, ph);
            ps_pmt.setString(2, sh);
            ps_pmt.setString(3, ptxid);
            ps_pmt.setBoolean(4, true);
            try {
                ps_pmt.executeUpdate();
            }
            catch (SQLException e) {
                if (!e
                    .getSQLState()
                    .equals(SQL_INTEGRITY_CONSTRAINT_VIOLATION))
                    throw e;
            }
        }
        ps_pt.close();
        ps_pmt.close();
    }

    private static void addMoment(Connection conn, String scr, String sh, String ptxid)
        throws SQLException {

	String st =
	    "INSERT INTO Scripts (s_hash, script) VALUES (?, ?)";
	String smt =
	    "INSERT INTO Script_Moments (s_hash, ptxid, timedelta, rwd, umask, complete) VALUES (?, ?, ?, ?, ?, ?)";

	//System.out.println(st);
	//System.out.println(smt);

	PreparedStatement ps_st = conn.prepareStatement(st);
	ps_st.clearParameters();
	ps_st.setString(1, sh);
	ps_st.setString(2, scr);
	try {
	    ps_st.executeUpdate();
	}
	catch (SQLException e) {
	    if (!e.getSQLState().equals(SQL_INTEGRITY_CONSTRAINT_VIOLATION))
		throw e;
	}
	finally {
	    ps_st.close();
	}

	PreparedStatement ps_smt = conn.prepareStatement(smt);
	ps_smt.clearParameters();
	ps_smt.setString(1, sh);
	ps_smt.setString(2, ptxid);
	ps_smt.setLong(3, 105);
	ps_smt.setString(4, "<rwd>");
	ps_smt.setInt(5, -2);
	ps_smt.setBoolean(6, true);
	try {
	    ps_smt.executeUpdate();
	}
	finally {
	    ps_smt.close();
	}
    }

    private static long slowQuery1(Connection conn, String sh)
        throws SQLException {

	String query = "SELECT *"
		     + " FROM Paths,Path_Moments,Script_Moments"
		     + " WHERE Path_Moments.s_hash = Script_Moments.s_hash"
		     + " AND Paths.p_hash = Path_Moments.p_hash"
		     + " AND Path_Moments.s_hash = " + sh
		     + " AND Script_Moments.rwd = ''"
		     + " ORDER BY Paths.path,Path_Moments.ptxid DESC";

        Statement stmt = conn.createStatement();
	long begin = System.currentTimeMillis();
        stmt.execute(query);
	return System.currentTimeMillis() - begin;
    }

    private static long slowQuery2(Connection conn, String sh)
        throws SQLException {

	String query = "SELECT path,ptxid,timedelta,umask,rwd,istarget"
		     + " FROM Paths,Path_Moments,Script_Moments"
		     + " WHERE Path_Moments.s_hash = Script_Moments.s_hash"
		     + " AND Paths.p_hash = Path_Moments.p_hash"
		     + " AND Path_Moments.s_hash = " + sh
		     + " AND Script_Moments.rwd = ''"
		     + " ORDER BY Paths.path,Path_Moments.ptxid DESC";

        Statement stmt = conn.createStatement();
	long begin = System.currentTimeMillis();
        stmt.execute(query);
	return System.currentTimeMillis() - begin;
    }

    private static long slowQuery3(Connection conn, String sh)
        throws SQLException {

	String query = "SELECT path,ptxid,timedelta,umask,rwd,istarget"
		     + " FROM Paths,Path_Moments,Script_Moments"
		     + " WHERE Paths.p_hash = Path_Moments.p_hash"
		     + " AND Path_Moments.s_hash = Script_Moments.s_hash"
		     + " AND Script_Moments.rwd = ''"
		     + " AND Path_Moments.s_hash = " + sh
		     + " ORDER BY Paths.path,Path_Moments.ptxid DESC";

        Statement stmt = conn.createStatement();
	long begin = System.currentTimeMillis();
        stmt.execute(query);
	return System.currentTimeMillis() - begin;
    }

    public static void main(String[] args)
	throws Exception
    {
	if (args.length != 2) {
	    System.err.println("Usage: SlowDown <reps> <pths>");
	    System.exit(1);
	}

	int reps = Integer.parseInt(args[0]);
	int pths = Integer.parseInt(args[1]);

	SimpleDateFormat sdf = new SimpleDateFormat("yyyyMMddHHmmss");
        sdf.setTimeZone(new SimpleTimeZone(0, "GMT"));
	java.util.Date now = new java.util.Date();
	String ptxid = sdf.format(now);

	Class.forName("org.hsqldb.jdbcDriver");
	Connection conn = DriverManager.getConnection("jdbc:hsqldb:SLOWDOWN",
							"sa", "");
	createDB(conn);

	for (int i = 0; i < reps; i++) {
	    //System.out.println("REP: " + i);
	    ptxid = String.valueOf(new Long(ptxid).longValue() + 1);

	    //System.out.println("Adding moment: " + ptxid);
	    addPTX(conn, ptxid);

	    String scr = String.valueOf(i);
	    scr = "a string";
	    String sh = new Long(Math.abs(scr.hashCode())).toString();
	    //System.out.println("Adding scr: " + scr);
	    addMoment(conn, scr, sh, ptxid);

	    ArrayList al = new ArrayList();
	    for (int j = 0; j < pths; j++) {
		String pth = scr + "." + String.valueOf(j);
		//System.out.println("Adding pth: " + pth);
		al.add(pth);
	    }
	    addPaths(conn, scr, sh, ptxid, al);

	    long duration = slowQuery1(conn, sh);
	    System.out.println("Q1 #" + i + "\t" + duration + "ms");

	    duration = slowQuery2(conn, sh);
	    System.out.println("Q2 #" + i + "\t" + duration + "ms");

	    duration = slowQuery3(conn, sh);
	    System.out.println("Q3 #" + i + "\t" + duration + "ms");

	    System.out.println("");
	}

	conn.close();
    }
}
