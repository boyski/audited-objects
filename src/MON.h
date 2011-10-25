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

#ifndef MON_H
#define MON_H

/// @file
/// @brief Declarations for mon.c

/// These are bits which may be set in the return code from mon_record().

/// Line parsed successfully, hit me again.
#define MON_NEXT			(1<<0)
/// The received line represents a top-level command.
#define MON_TOP				(1<<1)
/// The received line was a start-of-audit.
#define MON_SOA				(1<<2)
/// The received line was an end-of-audit.
#define MON_EOA				(1<<3)
/// The received line represents an aggregating command.
#define MON_AGG				(1<<4)
/// The received line represents a recycled command.
#define MON_RECYCLED			(1<<5)
/// The command was unable to fulfill a requirement and must die.
#define MON_STRICT			(1<<6)
/// An unspecified error occurred and has been handled.
#define MON_ERR				(1<<7)
/// The top-level monitored process was unable to start.
#define MON_CANTRUN			(1<<8)

extern void mon_init(void);
extern void mon_get_roadmap(void);
extern int mon_begin_session(void);
extern void mon_ptx_start(void);
extern void mon_dump(void);
extern unsigned mon_record(CS, int *, unsigned long *, CCS *);
extern void mon_ptx_end(int, CCS);
extern void mon_fini(void);

#endif
