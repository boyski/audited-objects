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

package com.aotool.entity;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class MomentTest {

    @Test
    public void exerciseMoment() {

        String str = "khv57g.b7g420";

        Moment moment1 = new Moment(str);
        long seconds = moment1.getSeconds();
        long nanos = moment1.getNanoseconds();

        assertEquals(str, moment1.toStandardString());

        Moment moment2 = new Moment(seconds, nanos);

        assertEquals(moment1, moment2);
    }
}
