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

package com.aotool.client;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;

public class FakeBuild {

    /**
     * @param args
     * 
     *            Usage: <code>java ThisClass <outputfile> <inputfile>...
     */
    public static void main(String[] args) throws Exception {
        byte[] buf = new byte[2048];
        for (int i = 1; i < args.length; i++) {
            File arg = new File(args[i]);
            if (arg.isDirectory()) {
                ArrayList<String> al = new ArrayList<String>();
                al.add(args[0]);
                String[] contents = arg.list();
                for (int j = 0; j < contents.length; j++) {
                    String path = new File(arg, contents[j]).getPath();
                    al.add(path);
                }
                main(al.toArray(new String[0]));
            } else {
                // System.out.println(args[i]);
                InputStream is = new FileInputStream(arg);
                OutputStream os = new FileOutputStream(args[0], false);
                for (int len = 0; (len = is.read(buf, 0, buf.length)) != -1;) {
                    os.write(buf, 0, len);
                }
                is.close();
                os.close();
            }
        }
    }
}
