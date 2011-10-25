/*
 * Copyright (C) 2006 David Boyce.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// This is an interposer object which rejects access to /proc/self/exe on Linux.
//    Suggested Linux compile line:
// gcc -o libnoself.so -D_GNU_SOURCE -g -W -Wall -fpic -shared -z interpose libnoself.c -ldl
//    Usage:
// LD_PRELOAD=/path/to/libnoself.so your-program-here arguments...
//    Example:
// LD_PRELOAD=./libnoself.so ls -l /proc/self/exe

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

// Here we assume that the file will be opened by its full path.
// Conceivably a program could chdir to /proc/self and read "exe"
// but there seems no reason to do so. 
#define HIDE_LINK		"/proc/self/exe"

int
readlink(const char *path, char *buf, size_t bufsiz)
{
    static int (*next) (const char *path, char *buf, size_t bufsiz);

    if (!next) {
	next = (int(*)(const char *,char *,size_t))dlsym(RTLD_NEXT, "readlink");
	if (!next) {
	    const char *err;
	    if ((err = dlerror()) == 0)
		err = "????";
	    fprintf(stderr, "dlsym(RTLD_NEXT,readlink): %s\n", err);
	    errno = EIO;	// random choice
	    return -1;
	}
    }

    if (path && !strcmp(path, HIDE_LINK)) {
	errno = ENOENT;		// would EACCES be better?
	return -1;
    } else {
	return (*next)(path, buf, bufsiz);
    }
}
