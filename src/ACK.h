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

#ifndef ACK_H
#define ACK_H

/// @file
/// @brief Declarations for data shared between the monitor and auditor.

/// A safe size for the ack buffer.
#define ACK_BUFFER_SIZE				256

/// Token sent from monitor to auditor saying "everything fine - go ahead"
#define ACK_OK					"-OK-"

/// Like ACK_OK but adds "this command is aggregated".
#define ACK_OK_AGG				"-OK_AGG-"

/// Token sent from monitor to auditor saying "failure detected - shut down"
#define ACK_FAILURE				"-FAILURE-"

#endif				/*ACK_H */
