// Copyright (c) 2005-2025 David Boyce.  All rights reserved.

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

#ifndef TRY_H
#define TRY_H

/// @file
/// @brief Support for the TRY() macro.

#include <errno.h>

// Get first argument of __VA_ARGS__ if any.
#define FIRST_ARG(...) FIRST_ARG__(__VA_ARGS__, throwaway)
#define FIRST_ARG__(first, ...) "" #first
// Print caller function name, file name, line_number, error message
// and optional supporting arguments. Exit with failure code.
// Supporting arguments are a printf() format string followed
// by respective printf() arguments.
#define DIE_(err_msg, ...) if (1) { \
    fprintf(stderr, "Error: %s() %s:%d %s", \
        __func__, __FILE__, __LINE__, err_msg); \
    if (errno) fprintf(stderr, " %s", strerror(errno)); \
    if (strcmp(FIRST_ARG(__VA_ARGS__), "")) \
        fprintf(stderr, ": " __VA_ARGS__); \
    fputc('\n', stderr); \
    if (getenv("AO_DEBUG")) abort(); \
    exit(EXIT_FAILURE); \
}

// Call the function and check whether condition is true.
// If not, call the DIE_() macro above with "cond" as a string
// and optional supporting arguments.
#define TRY(cond, ...) if (!(cond)) {DIE_(#cond, ##__VA_ARGS__);}

#endif // TRY_H

// vim: sw=4 ts=4 et tw=100 cc=+1
