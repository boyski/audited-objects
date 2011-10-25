// Copyright (c) 2005-2011 David Boyce.  All rights reserved.

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

#ifndef UW_H
#define UW_H

/// @file
/// @brief Declarations for functions defined in unix.c and win.c

/// Special "everything is done" marker.
#define DONE_TOKEN				"{DONE}"

/// Handle the actual running of an audited command. The implementation
/// of this is different between Unix and Windows.
/// @param[in] exe The name of the program to run
/// @param[in] argv The arg vector of the program to run
/// @param[in] logfile A pathname to send stdout and stderr to, or NULL
extern int run_cmd(CCS exe, CS *argv, CCS logfile);

#endif				/*UW_H */
