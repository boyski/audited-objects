// Copyright (c) 2002-2010 David Boyce.  All rights reserved.

/*
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
 */

#ifndef VB_H
#define VB_H

/// @file
/// @brief Declarations for vb.c

/**************************************************************************
* It's important to keep these defines and the vbtab array in sync!
* We also try to keep them in alphabetical order so the 'public'
* subset will print out alphabetically, with the exception of ON,
* OFF, STD, and TMP being at the beginning. A flag which will never be
* public need not be alphabetical and may be placed at the end.
* Some of these may be unused but kept for potential future need.
**************************************************************************/

/// @cond VB

/// Always on.
#define VB_ON			-1

/// Never on.
#define VB_OFF			-2

/// Standard verbosity (uploading/downloading messages).
#define VB_STD			0

/// Generally unused; available for temporary debugging scenarios.
#define VB_TMP			1

/// Show details of aggregation/disaggregation activity.
#define VB_AG			2

/// Show details of CmdAction activity.
#define VB_CA			3

/// Show details of libcurl activity.
#define VB_CURL			4

/// Print all commands executed.
#define VB_EXEC			5

/// Show full headers of all HTTP transactions.
#define VB_HTTP			6

/// Show details of monitor activity.
#define VB_MON			7

/// Show details of PathAction activity.
#define VB_PA			8

/// Trace shopping activity.
#define VB_SHOP			9

/// Show statistics on how much time is spent in HTTP transactions.
#define VB_TIME			10

/// Show the URLs of all HTTP transactions.
#define VB_URL			11

/// Explain why a prior command could not be recycled.
#define VB_WHY			12

/// For general purpose analysis within the regular expression subsystem.
#define VB_RE			13

/// Track file mapping and unmapping activity.
#define VB_MAP			14

/// For analysis within the uploading subsystem.
#define VB_UP			15

/// @endcond VB

#include <stdio.h>
#include <time.h>
#include <wchar.h>
#include <sys/stat.h>
#include <sys/types.h>

extern void vb_init(void);
extern FILE *vb_get_stream(void);
extern void vb_addstr(CCS);
extern void vb_addbit(int);
extern int vb_bitmatch(long);
extern void vb_printfA(long, const char *, ...);
#if defined(_WIN32)
extern void vb_printfW(long, const wchar_t *, ...);
#endif	/*_WIN32*/
extern void vb_fini(void);

/// @cond VB
#define vb_printf			vb_printfA
/// @endcond VB

#endif				/*VB_H */
