// Copyright (c) 2005-2010 David Boyce.  All rights reserved.

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

#ifndef SHOP_H
#define SHOP_H

/// @file
/// @brief Support for audited-object "shopping".

/// These are the return codes from shop().
/// They tell the shopping command whether it can exit or needs to
/// go ahead and run, and to a limited extent why.
// Might this be more cleanly handled as a bit mask?
typedef enum {
    SHOP_ERR,			///< Something went wrong
    SHOP_OFF,			///< Shopping is turned off
    SHOP_NOMATCH,		///< Cmd found but no PTX match
    SHOP_NOMATCH_AGG,		///< As above, but cmd was aggregated
    SHOP_MUSTRUN,		///< Cmd found but must be run anyway
    SHOP_MUSTRUN_AGG,		///< As above, but cmd was aggregated
    SHOP_RECYCLED,		///< Cmd successfully matched and recycled
} shop_e;

extern void shop_init(void);
extern shop_e shop(ca_o, CCS, int);
extern int shop_get_count(void);
extern void shop_fini(void);

#endif				/*SHOP_H */
