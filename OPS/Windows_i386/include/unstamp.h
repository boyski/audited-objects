#ifndef UNSTAMP_H
#define UNSTAMP_H

// Copyright (c) 2005-2010 David Boyce.  All rights reserved.

//#pragma comment(lib, "unstamp.lib")

#ifdef __cplusplus
extern "C" {
#endif

// Patch any timestamps or similar in an EXE/DLL/OBJ.
// If file format is not recognized the data is unmodified.
VOID unstamp_mapped_file(const unsigned char *);

#ifdef __cplusplus
}
#endif

#endif /*UNSTAMP_H*/
