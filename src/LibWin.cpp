// Copyright (c) 2002-2011 David Boyce.  All rights reserved.

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

/// @file
/// @brief The part of the auditor library which is specific
/// to Windows.

// We need (at least) GetVolumePathName*() and friends
#define _WIN32_WINNT 0x0500

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <WindowsX.h>

#include <assert.h>
#include <malloc.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <tlhelp32.h>
#include <psapi.h>
//#pragma comment(lib, "Psapi.lib")

/// @cond static
#define PUTIL_DECLARE_FUNCTIONS_STATIC
#include "Putil/putil.h"
/// @endcond

#include "AO.h"

// Make up our own bogus "access right" to mean "just unlinked".
#define GENERIC_UNLINK		((DWORD)-1)

// Must be declared before dragging in the code below.
static int _ignore_path(const char *);
static void _thread_mutex_lock(void);
static void _thread_mutex_unlock(void);

HANDLE WINAPI
CreateFileA_real(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
		 DWORD, DWORD, HANDLE);
HANDLE WINAPI
CreateFileW_real(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
		 DWORD, DWORD, HANDLE);

// This drags in a LOT of other code ...
#include "libcommon.c"

#include "Toolhelp.H"

#include "LibAO.H"

extern "C" {
    LIBAO_API BOOL LDPInjectW(LPPROCESS_INFORMATION, LPCWSTR, LPCSTR, LPCSTR);
    LIBAO_API void PreMain(void);
}

#include <ImageHlp.h>

#pragma comment(lib, "ImageHlp")

#include "APIHook.H"

// Prototypes for the hooked functions

typedef HANDLE(WINAPI *PFNCREATEFILEA)
(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE(WINAPI *PFNCREATEFILEW)
(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

typedef BOOL(WINAPI *PFNDELETEFILEA)
(LPCSTR);
typedef BOOL(WINAPI *PFNDELETEFILEW)
(LPCWSTR);

typedef BOOL(WINAPI *PFNCREATEPROCESSW)
(LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
 BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFO, LPPROCESS_INFORMATION);
typedef BOOL(WINAPI *PFNCREATEPROCESSA)
(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
 BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFO, LPPROCESS_INFORMATION);

typedef BOOL(WINAPI *PFNCREATEPROCESSASUSERW)
(HANDLE, LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
 BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFO, LPPROCESS_INFORMATION);
typedef BOOL(WINAPI *PFNCREATEPROCESSASUSERA)
(HANDLE, LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
 BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFO, LPPROCESS_INFORMATION);

typedef BOOL(WINAPI *PFNCREATEPROCESSWITHLOGONW)
(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPCWSTR, LPWSTR,
 DWORD, LPVOID, LPCWSTR, LPSTARTUPINFO, LPPROCESS_INFORMATION);

typedef BOOL(WINAPI *PFNCREATEPROCESSWITHTOKENW)
(HANDLE, DWORD, LPCWSTR, LPWSTR,
 DWORD, LPVOID, LPCWSTR, LPSTARTUPINFO, LPPROCESS_INFORMATION);

typedef VOID(WINAPI *PFNEXITPROCESS)
(UINT);

typedef BOOL(WINAPI *GETMODINFO)(HANDLE, HMODULE, LPMODULEINFO, DWORD);

typedef HRESULT (STDAPICALLTYPE *PDLLREGISTERSERVER)();
typedef HRESULT (STDAPICALLTYPE *PDLLUNREGISTERSERVER)();

// These hold references to the objects managing each hook.
// They will be destructed automatically upon going out of scope,
// which is generally at DLL-detach time.
static CAPIHook *g_CreateFileA, *g_CreateFileW;
static CAPIHook *g_DeleteFileA, *g_DeleteFileW;
static CAPIHook *g_CreateProcessA, *g_CreateProcessW;
static CAPIHook *g_CreateProcessAsUserA, *g_CreateProcessAsUserW;
static CAPIHook *g_CreateProcessWithLogonW;
static CAPIHook *g_CreateProcessWithTokenW;
static CAPIHook *g_ExitProcess;

// Holds the name (actually, full path) of this DLL.
static WCHAR szDllPathW[MAX_PATH];

static char szThisProgPath[MAX_PATH];

static CRITICAL_SECTION StaticDataInitCriticalSection;

static PDLLUNREGISTERSERVER DllUnRegisterServer;

/// Convert an ANSI (narrow) string to its Unicode equivalent on the stack.
/// Note that this declares a variable to hold the new value.
#define A2W(astr, wstr) \
    LPWSTR wstr; \
    if (astr) { \
	int widelen = MultiByteToWideChar(CP_ACP, 0, astr, -1, NULL, 0); \
	wstr = (LPWSTR) _alloca(widelen * sizeof(WCHAR)); \
	widelen = MultiByteToWideChar(CP_ACP, 0, astr, -1, wstr, widelen); \
	assert(widelen != 0); \
    } else { \
	wstr = NULL; \
    }

/// Convert a Unicode (wide) string to its UTF-8 equivalent on the stack.
/// Note that this declares a variable to hold the new value.
#define W2A(wstr, astr) \
    LPSTR astr; \
    if (wstr) { \
	int utf8len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, \
	    NULL, 0, NULL, NULL); \
	astr = (LPSTR)_alloca(utf8len * sizeof(char)); \
	utf8len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, \
	    astr, utf8len, NULL, NULL); \
	assert(utf8len != 0); \
    } else { \
	astr = NULL; \
    }

// If we need to open a file ourselves within this lib, typically to
// create a temp file, we can use these to get the "real" Win32 functions.

HANDLE WINAPI
CreateFileA_real(LPCSTR lpFileName, DWORD dwDesiredAccess,
		 DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		 DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
		 HANDLE hTemplateFile)
{
    return ((PFNCREATEFILEA) (PROC) *g_CreateFileA)
	(lpFileName, dwDesiredAccess, dwShareMode,
	lpSecurityAttributes, dwCreationDisposition,
	dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI
CreateFileW_real(LPCWSTR lpFileName, DWORD dwDesiredAccess,
		 DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		 DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
		 HANDLE hTemplateFile)
{
    return ((PFNCREATEFILEW) (PROC) *g_CreateFileW)
	(lpFileName, dwDesiredAccess, dwShareMode,
	lpSecurityAttributes, dwCreationDisposition,
	dwFlagsAndAttributes, hTemplateFile);
}

// Fill in the provided buffer with the path to this DLL.
// Must be done in a separate function because it uses its
// own address as a reference point.
static void
_GetThisDllNameW(LPWSTR buf, int len)
{
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(_GetThisDllNameW, &mbi, sizeof(mbi));
    HMODULE hModule = (HMODULE) mbi.AllocationBase;
    DWORD rc = GetModuleFileNameW(hModule, buf, len);
    assert(rc != 0);
}

// This does not use TCHARs because we assume UTF-8.
static int
_ignore_path(const char *path)
{
    int rc = false;

    // Apparently MS builders keep "BuildLog.htm" open with a lock
    // till the bitter end of the process such that we can't
    // even derive the data code. Simplest fix is to ignore it which
    // is probably the right thing anyway. Similar for "index.dat"
    // files. *&%# Windows. These are handled via IgnorePathRE.

    if (util_is_tmp(path)) {
	// Ignore temp files.
	rc = true;
    } else if (IgnorePathRE && re_match__(IgnorePathRE, path)) {
	// User-configurable set of files to be completely ignored.
	rc = 1;
    }

    return rc;
}

static inline void
_thread_mutex_lock(void)
{
    EnterCriticalSection(&StaticDataInitCriticalSection);
}

static inline void
_thread_mutex_unlock(void)
{
    LeaveCriticalSection(&StaticDataInitCriticalSection);
}

static void
_RecordFile(LPCSTR lpCall, LPCSTR lpFileName,
	    DWORD dwDesiredAccess, DWORD dwCreationDisposition)
{
    op_e op;

    if ((dwDesiredAccess == GENERIC_UNLINK)) {
	op = OP_UNLINK;
    } else if ((dwDesiredAccess & (FILE_APPEND_DATA))) {
	op = OP_APPEND;
    } else if ((dwDesiredAccess &
	(GENERIC_WRITE|GENERIC_ALL|FILE_WRITE_DATA|FILE_ALL_ACCESS))) {
	    // It seems possible a sloppy programmer might open files
	    // with "all access" rights but only read from them. Such as
	    // the author of Notepad, for instance! Try to deal with it.
	    if (dwCreationDisposition == OPEN_EXISTING) {
		op = OP_READ;
	    } else {
		op = OP_CREAT;
	    }
    } else if ((dwDesiredAccess & (GENERIC_EXECUTE|FILE_EXECUTE))) {
	op = OP_EXEC;
    } else if ((dwDesiredAccess & (GENERIC_READ|FILE_READ_DATA))) {
	op = OP_READ;
    }

    // Make the pathname absolute.
    char abspath[MAX_PATH*5];
    if (!_fullpath(abspath, lpFileName, sizeof(abspath))) {
	putil_win32err(0, GetLastError(), lpFileName);
	strcpy(abspath, lpFileName);
    }

    // Convert it to a canonical case representation.
    DWORD len = GetLongPathNameA(abspath, abspath, sizeof(abspath));
    // We do not give an error on len==0 because this most
    // often happens when the file has just been unlinked.
    if (len > sizeof(abspath)) {
	putil_win32err(0, GetLastError(), abspath);
    }

    // Unfortunately, it appears that although GetLongPathName will
    // canonicalize the path it doesn't touch the drive letter.
    // Therefore we must intercede with a standard of our own
    // by forcing it to upper case.
    if (isalpha((int)abspath[0])
	&& islower((int)abspath[0])
	&& abspath[1] == ':') {
	    abspath[0] = _totupper((int)abspath[0]);
    }

    // Now we have a case-canonicalized UTF-8 absolute pathname.
    _pa_record(lpCall, abspath, NULL, -1, op);
}

HANDLE WINAPI
Hook_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess,
		 DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		 DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
		 HANDLE hTemplateFile)
{
    // Call the original function
    HANDLE hResult = ((PFNCREATEFILEW) (PROC) *g_CreateFileW)
	(lpFileName, dwDesiredAccess, dwShareMode,
	lpSecurityAttributes, dwCreationDisposition,
	dwFlagsAndAttributes, hTemplateFile);

    // Don't bother recording these situations.
    if (hResult == INVALID_HANDLE_VALUE || dwDesiredAccess == 0) {
	return hResult;
    }

    // Must save this before doing anything which might change it.
    DWORD dwLastError = GetLastError();

    // Add this file to the audit data structure.
    if (GetFileType(hResult) == FILE_TYPE_DISK) {
	W2A(lpFileName, lpFileNameA);
	_RecordFile("CreateFileW", lpFileNameA,
	    dwDesiredAccess, dwCreationDisposition);
    }

    // Restore error status from hooked ("real") function
    SetLastError(dwLastError);

    return (hResult);
}

HANDLE WINAPI
Hook_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess,
		 DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		 DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
		 HANDLE hTemplateFile)
{
    // Call the original function
    HANDLE hResult = ((PFNCREATEFILEA) (PROC) *g_CreateFileA)
	(lpFileName, dwDesiredAccess, dwShareMode,
	lpSecurityAttributes, dwCreationDisposition,
	dwFlagsAndAttributes, hTemplateFile);

    // Don't bother recording these situations.
    if (hResult == INVALID_HANDLE_VALUE || dwDesiredAccess == 0) {
	return hResult;
    }

    // Must save this before doing anything which might change it.
    DWORD dwLastError = GetLastError();

    // Add this file to the audit data structure.
    if (GetFileType(hResult) == FILE_TYPE_DISK) {
	_RecordFile("CreateFileA", lpFileName,
	    dwDesiredAccess, dwCreationDisposition);
    }

    // Restore error status from hooked ("real") function
    SetLastError(dwLastError);

    return (hResult);
}

BOOL WINAPI
Hook_DeleteFileW(LPCWSTR lpFileName)
{
    // Call the original function
    BOOL bResult = ((PFNDELETEFILEW) (PROC) *g_DeleteFileW) (lpFileName);

    // No need to record unsuccessful unlinks.
    if (bResult == 0) {
	return bResult;
    }

    // Add this ex-file to the audit data structure.
    W2A(lpFileName, lpFileNameA);
    _RecordFile("DeleteFileW", lpFileNameA, GENERIC_UNLINK, (DWORD)-1);

    return bResult;
}

BOOL WINAPI
Hook_DeleteFileA(LPCSTR lpFileName)
{
    // Call the original function
    BOOL bResult = ((PFNDELETEFILEA) (PROC) *g_DeleteFileA) (lpFileName);

    // No need to record unsuccessful unlinks.
    if (bResult == 0) {
	return bResult;
    }

    // Add this ex-file to the audit data structure.
    _RecordFile("DeleteFileA", lpFileName, GENERIC_UNLINK, (DWORD)-1);

    return bResult;
}

// Collects all the common injection code into one place.
static DWORD
create_process_commonW(
			     LPCWSTR lpApplicationName,
			     LPWSTR lpCommandLine,
			     LPCWSTR lpHookedFunction,
			     LPPROCESS_INFORMATION lpProcessInformation,
			     DWORD dwCreationFlags)
{
    LPWSTR exeNameW;

    // The executable name might be found in either of these strings ...
    if (lpApplicationName) {
	exeNameW = (LPWSTR)lpApplicationName;
    } else {
	LPWSTR p;

	// We could use CommandLineToArgvW() but that drags in all
	// sorts of shell32.lib stuff. Trying to do without.
	exeNameW = (LPWSTR)_alloca((wcslen(lpCommandLine) + 1) * sizeof(WCHAR));
	if (lpCommandLine[0] == '"') {
	    wcscpy(exeNameW, lpCommandLine + 1);
	    for (p = exeNameW; *p; p++) {
		if (p[0] == '"' && p[1] != '"') {
		    p[0] = '\0';
		    break;
		}
	    }
	} else {
	    wcscpy(exeNameW, lpCommandLine);
	    for (p = exeNameW; *p; p++) {
		if (iswspace(*p)) {
		    p[0] = '\0';
		    break;
		}
	    }
	}
    }

    //fwprintf(stderr, L"INJECTING %s: %s(%s)\n", exeNameW, lpHookedFunction, lpCommandLine);

    // Now we're ready to proceed with the actual injection.
    W2A(szDllPathW, szDllPathA);
    if (!LDPInjectW(lpProcessInformation, exeNameW, szDllPathA, PREMAIN)) {
	putil_win32errW(0, GetLastError(), szDllPathW);
    }

    // Last, let it run (unless the original request was for
    // a suspended process).
    if (!(dwCreationFlags & CREATE_SUSPENDED)) {
	if (ResumeThread(lpProcessInformation->hThread) == -1) {
	    return -1;
	}
    }

    return 0;
}

BOOL WINAPI
Hook_CreateProcessW(
		    LPCWSTR lpApplicationName,
		    LPWSTR lpCommandLine,
		    LPSECURITY_ATTRIBUTES lpProcessAttributes,
		    LPSECURITY_ATTRIBUTES lpThreadAttributes,
		    BOOL bInheritHandles,
		    DWORD dwCreationFlags,
		    LPVOID lpEnv,
		    LPCWSTR lpCurrentDirectory,
		    LPSTARTUPINFO lpStartupInfo,
		    LPPROCESS_INFORMATION lpProcessInformation)
{
    if (IgnoreProgRE) {
	W2A(lpApplicationName, exeNameA);
	if (re_match__(IgnoreProgRE, exeNameA)) {
	    vb_printf(VB_TMP, "IGNORING %s", exeNameA);
	    return ((PFNCREATEPROCESSW) (PROC) *g_CreateProcessW)
		(lpApplicationName, lpCommandLine,
		lpProcessAttributes, lpThreadAttributes,
		bInheritHandles, dwCreationFlags, lpEnv,
		lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
	}
    }

    // Put the pid of the *current* process into the env block as the
    // *parent* cmd id. We rely on the standard Windows APIs to
    // make sure this gets into both A and W environment blocks.
    prop_mod_ulong(P_PCMDID, GetCurrentProcessId(), NULL);

    // Must flush the beginnings of the audit before spawning any
    // new processes. Aggregation depends on seeing the cmd line
    // before any child audits arrive.
    _audit_end("CreateProcessW", NOT_EXITING, 0);

    LPVOID lpEnvPlusProps = NULL;

    // "If you lie to the compiler, it will get its revenge". Beware.
    if (lpEnv) {
	if (dwCreationFlags & CREATE_UNICODE_ENVIRONMENT) {
	    size_t plen = prop_new_env_block_sizeW((WCHAR *const *)lpEnv);
	    WCHAR *const *lpBlock = (WCHAR *const *)_alloca(plen);
	    memset((void *)lpBlock, 0, plen);
	    (void)prop_custom_envW((WCHAR **)lpBlock, (WCHAR *const *)lpEnv);
	    lpEnvPlusProps = lpBlock ? lpBlock[0] : NULL;
	} else {
	    size_t plen = prop_new_env_block_sizeA((char *const *)lpEnv);
	    char *const *lpBlock = (char *const *)_alloca(plen);
	    memset((void *)lpBlock, 0, plen);
	    (void)prop_custom_envA((char **)lpBlock, (char *const *)lpEnv);
	    lpEnvPlusProps = lpBlock ? lpBlock[0] : NULL;
	}
    }

    // Call the original function. Note that the new process
    // will inherit updated depth and ppid properties.
    BOOL bResult = ((PFNCREATEPROCESSW) (PROC) *g_CreateProcessW)
	(lpApplicationName, lpCommandLine,
	lpProcessAttributes, lpThreadAttributes,
	bInheritHandles,
	dwCreationFlags | CREATE_SUSPENDED,
	lpEnvPlusProps,
	lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    // If process creation failed, return the error state.
    if (bResult == 0) {
	return bResult;
    }

    // Now, while the new process is suspended, do the DLL-insertion magic.
    if (create_process_commonW(lpApplicationName, lpCommandLine,
	    L"CreateProcessW", lpProcessInformation, dwCreationFlags)) {
	return -1;
    }

    return bResult;
}

BOOL WINAPI
Hook_CreateProcessA(
		    LPCSTR lpApplicationName,
		    LPSTR lpCommandLine,
		    LPSECURITY_ATTRIBUTES lpProcessAttributes,
		    LPSECURITY_ATTRIBUTES lpThreadAttributes,
		    BOOL bInheritHandles,
		    DWORD dwCreationFlags,
		    LPVOID lpEnv,
		    LPCSTR lpCurrentDirectory,
		    LPSTARTUPINFO lpStartupInfo,
		    LPPROCESS_INFORMATION lpProcessInformation)
{
    if (IgnoreProgRE) {
	if (re_match__(IgnoreProgRE, lpApplicationName)) {
	    vb_printf(VB_TMP, "IGNORING %s", lpApplicationName);
	    return ((PFNCREATEPROCESSA) (PROC) *g_CreateProcessA)
		(lpApplicationName, lpCommandLine,
		lpProcessAttributes, lpThreadAttributes,
		bInheritHandles, dwCreationFlags, lpEnv,
		lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
	}
    }

    // Convert the ANSI app name to its Unicode equivalent on the stack
    A2W(lpApplicationName, lpApplicationNameW);

    // Convert the ANSI cmdline to its Unicode equivalent on the stack
    A2W(lpCommandLine, lpCommandLineW);

    // Convert the ANSI cwd to its Unicode equivalent on the stack
    A2W(lpCurrentDirectory, lpCurrentDirectoryW);

    return Hook_CreateProcessW(lpApplicationNameW, lpCommandLineW,
	lpProcessAttributes, lpThreadAttributes,
	bInheritHandles, dwCreationFlags, lpEnv,
	lpCurrentDirectoryW, lpStartupInfo, lpProcessInformation);
}

BOOL WINAPI
Hook_CreateProcessAsUserW(
			  HANDLE hToken,
			  LPCWSTR lpApplicationName,
			  LPWSTR lpCommandLine,
			  LPSECURITY_ATTRIBUTES lpProcessAttributes,
			  LPSECURITY_ATTRIBUTES lpThreadAttributes,
			  BOOL bInheritHandles,
			  DWORD dwCreationFlags,
			  LPVOID lpEnv,
			  LPCWSTR lpCurrentDirectory,
			  LPSTARTUPINFO lpStartupInfo,
			  LPPROCESS_INFORMATION lpProcessInformation)
{
    if (IgnoreProgRE) {
	W2A(lpApplicationName, exeNameA);
	if (re_match__(IgnoreProgRE, exeNameA)) {
	    vb_printf(VB_TMP, "IGNORING %s", exeNameA);
	    return ((PFNCREATEPROCESSASUSERW) (PROC) *g_CreateProcessAsUserW)
		(hToken, lpApplicationName, lpCommandLine,
		lpProcessAttributes, lpThreadAttributes,
		bInheritHandles, dwCreationFlags, lpEnv,
		lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
	}
    }

    // Put the pid of the *current* process into the env block as the
    // *parent* cmd id.
    prop_mod_ulong(P_PCMDID, GetCurrentProcessId(), NULL);

    // Must flush the beginnings of the audit before spawning any
    // new processes. Aggregation depends on seeing the cmd line
    // before any child audits arrive.
    _audit_end("CreateProcessAsUser", NOT_EXITING, 0);

    LPVOID lpEnvPlusProps = NULL;

    // "If you lie to the compiler, it will get its revenge". Beware.
    if (lpEnv) {
	if (dwCreationFlags & CREATE_UNICODE_ENVIRONMENT) {
	    size_t plen = prop_new_env_block_sizeW((WCHAR *const *)lpEnv);
	    WCHAR *const *lpBlock = (WCHAR *const *)_alloca(plen);
	    memset((void *)lpBlock, 0, plen);
	    (void)prop_custom_envW((WCHAR **)lpBlock, (WCHAR *const *)lpEnv);
	    lpEnvPlusProps = lpBlock ? lpBlock[0] : NULL;
	} else {
	    size_t plen = prop_new_env_block_sizeA((char *const *)lpEnv);
	    char *const *lpBlock = (char *const *)_alloca(plen);
	    memset((void *)lpBlock, 0, plen);
	    (void)prop_custom_envA((char **)lpBlock, (char *const *)lpEnv);
	    lpEnvPlusProps = lpBlock ? lpBlock[0] : NULL;
	}
    }

    // Call the original function. Note that the new process
    // will inherit updated depth and ppid properties.
    BOOL bResult =
	((PFNCREATEPROCESSASUSERW) (PROC) *g_CreateProcessAsUserW)
	(hToken, lpApplicationName, lpCommandLine,
	lpProcessAttributes, lpThreadAttributes,
	bInheritHandles, dwCreationFlags | CREATE_SUSPENDED, lpEnvPlusProps,
	lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    // If process creation failed, return the error state.
    if (bResult == 0) {
	return bResult;
    }

    // Now, while the new process is suspended, do the DLL-insertion magic.
    if (create_process_commonW(lpApplicationName, lpCommandLine,
	    L"CreateProcessAsUserW", lpProcessInformation, dwCreationFlags)) {
	return -1;
    }

    return bResult;
}

BOOL WINAPI
Hook_CreateProcessAsUserA(
			  HANDLE hToken,
			  LPCSTR lpApplicationName,
			  LPSTR lpCommandLine,
			  LPSECURITY_ATTRIBUTES lpProcessAttributes,
			  LPSECURITY_ATTRIBUTES lpThreadAttributes,
			  BOOL bInheritHandles,
			  DWORD dwCreationFlags,
			  LPVOID lpEnv,
			  LPCSTR lpCurrentDirectory,
			  LPSTARTUPINFO lpStartupInfo,
			  LPPROCESS_INFORMATION lpProcessInformation)
{
    // Convert the ANSI app name to its Unicode equivalent on the stack
    A2W(lpApplicationName, lpApplicationNameW);

    // Convert the ANSI cmdline to its Unicode equivalent on the stack
    A2W(lpCommandLine, lpCommandLineW);

    // Convert the ANSI cwd to its Unicode equivalent on the stack
    A2W(lpCurrentDirectory, lpCurrentDirectoryW);

    return Hook_CreateProcessAsUserW(hToken, lpApplicationNameW, lpCommandLineW,
	lpProcessAttributes, lpThreadAttributes,
	bInheritHandles, dwCreationFlags, lpEnv,
	lpCurrentDirectoryW, lpStartupInfo, lpProcessInformation);
}

BOOL WINAPI
Hook_CreateProcessWithLogonW(
			     LPCWSTR lpUsername,
			     LPCWSTR lpDomain,
			     LPCWSTR lpPassword,
			     DWORD dwLogonFlags,
			     LPCWSTR lpApplicationName,
			     LPWSTR lpCommandLine,
			     DWORD dwCreationFlags,
			     LPVOID lpEnv,
			     LPCWSTR lpCurrentDirectory,
			     LPSTARTUPINFO lpStartupInfo,
			     LPPROCESS_INFORMATION lpProcessInformation)
{
    if (IgnoreProgRE) {
	W2A(lpApplicationName, exeNameA);
	if (re_match__(IgnoreProgRE, exeNameA)) {
	    vb_printf(VB_TMP, "IGNORING %s", exeNameA);
	    return ((PFNCREATEPROCESSWITHLOGONW) (PROC) *g_CreateProcessWithLogonW)
		(lpUsername, lpDomain, lpPassword, dwLogonFlags,
		lpApplicationName, lpCommandLine,
		dwCreationFlags, lpEnv,
		lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
	}
    }

    // Put the pid of the *current* process into the env block as the
    // *parent* cmd id.
    prop_mod_ulong(P_PCMDID, GetCurrentProcessId(), NULL);

    // Must flush the beginnings of the audit before spawning any
    // new processes. Aggregation depends on seeing the cmd line
    // before any child audits arrive.
    _audit_end("CreateProcessWithLogonW", NOT_EXITING, 0);

    LPVOID lpEnvPlusProps = NULL;

    // "If you lie to the compiler, it will get its revenge". Beware.
    if (lpEnv) {
	if (dwCreationFlags & CREATE_UNICODE_ENVIRONMENT) {
	    size_t plen = prop_new_env_block_sizeW((WCHAR *const *)lpEnv);
	    WCHAR *const *lpBlock = (WCHAR *const *)_alloca(plen);
	    memset((void *)lpBlock, 0, plen);
	    (void)prop_custom_envW((WCHAR **)lpBlock, (WCHAR *const *)lpEnv);
	    lpEnvPlusProps = lpBlock ? lpBlock[0] : NULL;
	} else {
	    size_t plen = prop_new_env_block_sizeA((char *const *)lpEnv);
	    char *const *lpBlock = (char *const *)_alloca(plen);
	    memset((void *)lpBlock, 0, plen);
	    (void)prop_custom_envA((char **)lpBlock, (char *const *)lpEnv);
	    lpEnvPlusProps = lpBlock ? lpBlock[0] : NULL;
	}
    }

    // Call the original function. Note that the new process
    // will inherit updated depth and ppid properties.
    BOOL bResult =
	((PFNCREATEPROCESSWITHLOGONW) (PROC) *g_CreateProcessWithLogonW)
	(lpUsername, lpDomain, lpPassword, dwLogonFlags,
	lpApplicationName, lpCommandLine,
	dwCreationFlags | CREATE_SUSPENDED, lpEnvPlusProps,
	lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    // If process creation failed, return the error state.
    if (bResult == 0) {
	return bResult;
    }

    // Now, while the new process is suspended, do the DLL-insertion magic.
    if (create_process_commonW(lpApplicationName, lpCommandLine,
	    L"CreateProcessWithLogonW", lpProcessInformation, dwCreationFlags)) {
	return -1;
    }

    return bResult;
}

BOOL WINAPI
Hook_CreateProcessWithTokenW(
			     HANDLE hToken,
			     DWORD dwLogonFlags,
			     LPCWSTR lpApplicationName,
			     LPWSTR lpCommandLine,
			     DWORD dwCreationFlags,
			     LPVOID lpEnv,
			     LPCWSTR lpCurrentDirectory,
			     LPSTARTUPINFO lpStartupInfo,
			     LPPROCESS_INFORMATION lpProcessInformation)
{
    if (IgnoreProgRE) {
	W2A(lpApplicationName, exeNameA);
	if (re_match__(IgnoreProgRE, exeNameA)) {
	    vb_printf(VB_TMP, "IGNORING %s", exeNameA);
	    return ((PFNCREATEPROCESSWITHTOKENW) (PROC) *g_CreateProcessWithTokenW)
		(hToken, dwLogonFlags,
		lpApplicationName, lpCommandLine,
		dwCreationFlags, lpEnv,
		lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
	}
    }

    // Put the pid of the *current* process into the env block as the
    // *parent* cmd id.
    prop_mod_ulong(P_PCMDID, GetCurrentProcessId(), NULL);

    // Must flush the beginnings of the audit before spawning any
    // new processes. Aggregation depends on seeing the cmd line
    // before any child audits arrive.
    _audit_end("CreateProcessWithTokenW", NOT_EXITING, 0);

    LPVOID lpEnvPlusProps = NULL;

    // "If you lie to the compiler, it will get its revenge". Beware.
    if (lpEnv) {
	if (dwCreationFlags & CREATE_UNICODE_ENVIRONMENT) {
	    size_t plen = prop_new_env_block_sizeW((WCHAR *const *)lpEnv);
	    WCHAR *const *lpBlock = (WCHAR *const *)_alloca(plen);
	    memset((void *)lpBlock, 0, plen);
	    (void)prop_custom_envW((WCHAR **)lpBlock, (WCHAR *const *)lpEnv);
	    lpEnvPlusProps = lpBlock ? lpBlock[0] : NULL;
	} else {
	    size_t plen = prop_new_env_block_sizeA((char *const *)lpEnv);
	    char *const *lpBlock = (char *const *)_alloca(plen);
	    memset((void *)lpBlock, 0, plen);
	    (void)prop_custom_envA((char **)lpBlock, (char *const *)lpEnv);
	    lpEnvPlusProps = lpBlock ? lpBlock[0] : NULL;
	}
    }

    // Call the original function. Note that the new process
    // will inherit updated depth and ppid properties.
    BOOL bResult =
	((PFNCREATEPROCESSWITHTOKENW) (PROC) *g_CreateProcessWithTokenW)
	(hToken, dwLogonFlags,
	lpApplicationName, lpCommandLine,
	dwCreationFlags | CREATE_SUSPENDED, lpEnvPlusProps,
	lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    // If process creation failed, return the error state.
    if (bResult == 0) {
	return bResult;
    }

    // Now, while the new process is suspended, do the DLL-insertion magic.
    if (create_process_commonW(lpApplicationName, lpCommandLine,
	    L"CreateProcessWithTokenW", lpProcessInformation, dwCreationFlags)) {
	return -1;
    }

    return bResult;
}

VOID WINAPI
Hook_ExitProcess(UINT uExitCode)
{
    // Undo hooks before writing audit results. May not be necessary.
    g_CreateFileA->~CAPIHook();
    g_CreateFileW->~CAPIHook();
    g_DeleteFileA->~CAPIHook();
    g_DeleteFileW->~CAPIHook();
    g_CreateProcessA->~CAPIHook();
    g_CreateProcessW->~CAPIHook();
    g_CreateProcessAsUserA->~CAPIHook();
    g_CreateProcessAsUserW->~CAPIHook();
    g_CreateProcessWithLogonW->~CAPIHook();
    g_CreateProcessWithTokenW->~CAPIHook();
    g_ExitProcess->~CAPIHook();

    // If a COM DLL was registered, unregister it here.
    // May be unnecessary.
    if(DllUnRegisterServer) {
	(*DllUnRegisterServer)();
    }

    // Do the final flush.
    _audit_end("exit", EXITING, uExitCode);

    // This would be done by exit anyway but what the heck.
    code_fini();

    // Shouldn't need our properties any more.
    prop_fini();

    // This would be done by exit anyway but what the heck.
    vb_fini();

    // Call the original function
    ((PFNEXITPROCESS) (PROC) *g_ExitProcess)(uExitCode);
}

///////////////////////////////////////////////////////////////////////////////

LIBAO_API void
PreMain(void)
{
    // Might as well initialize some static stuff while we
    // know we're single threaded. There are strict
    // limits about what can be done during DLL initialization
    // but AFAICT there are no such restrictions here.

    // This will be used to synchronize access to this library's
    // static data where required. Most static data is initialized
    // while still single threaded and synchronization is not
    // required there, but there are a few places "post main"
    // where we need it.
    InitializeCriticalSection(&StaticDataInitCriticalSection);

    // This relies on static storage so we call it once while
    // single threaded to make sure it's safely initialized.
    (void)util_is_tmp(".");

    // Determine the name of the DLL we're in.
    _GetThisDllNameW(szDllPathW, charlenW(szDllPathW));

    // The full path to the currently-running program.
    if (!GetModuleFileName(NULL, szThisProgPath, charlen(szThisProgPath))) {
	strcpy(szThisProgPath, "<UNKNOWN>");
    }

    // The command line, of course. We may be modifying it so we'll
    // need a temporary copy.
    CS xcmd = GetCommandLine();
    CS cmd = (CS) _alloca(strlen(xcmd) + sizeof(char));
    strcpy(cmd, xcmd);

    // Expand response files, aka command files, into the cmdline.
    // They cannot be left in the cmdline because they're often
    // temporary files with generated names like '_CL_daa01104db.rsp'.
    // So we pretend the same args were passed literally, which
    // will only cause trouble if someone later tries to cut and paste
    // a cmd into a shell AND it's longer than the shell allows.
    // Note 1: We try to do everything using Win32 APIs from kernel32.dll.
    // Note 2: this doesn't handle response file names with embedded
    // whitespace. Fix when and if it becomes a problem.
    if (strchr(cmd, '@')) {
	int i, j;
	char rspfile[MAX_PATH];
	for (i=0; cmd[i]; i++) {
	    if (cmd[i] == '@' && ISSPACE(cmd[i-1]) && !ISSPACE(cmd[i+1])) {

		// Find the other end of the filename.
		for (j=i+1; cmd[j] && !ISSPACE(cmd[j]); j++);

		// Copy it into a separate buffer.
		strncpy(rspfile, cmd + i + 1, j - i);
		rspfile[j - i] = '\0';

		// Open the response file.
		HANDLE hFile;
		hFile = CreateFileA(rspfile, GENERIC_READ, FILE_SHARE_READ,
		    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
		    putil_win32err(0, GetLastError(), rspfile);
		    continue;
		}

		// Assume the response file won't be >2GB.
		DWORD filsz = GetFileSize(hFile, NULL);
		if (filsz == INVALID_FILE_SIZE) {
		    putil_win32err(0, GetLastError(), rspfile);
		    continue;
		}

		// Copy the contents of the file into a buffer.
		DWORD nread;
		LPVOID rspbuf = _alloca(filsz + sizeof(WCHAR));
		ZeroMemory(rspbuf, filsz + sizeof(WCHAR));
		if (!ReadFile(hFile, rspbuf, filsz, &nread, NULL)) {
		    putil_win32err(0, GetLastError(), rspfile);
		    CloseHandle(hFile);
		    continue;
		}
		CloseHandle(hFile);

		// Assign a string pointer for convenience.
		LPSTR rspstr = (LPSTR)rspbuf;

		// If the response file data is in Unicode format and we're not,
		// it must be converted.
		if (IsTextUnicode(rspbuf, nread, 0)) {
		    W2A(((LPCWSTR)rspbuf) + 1, rspstr1);
		    rspstr = rspstr1;
		}

		// The new buffer can't be any larger than the old one
		// plus the size of the data read from the response file.
		DWORD nlen = bytelen(cmd) + nread + sizeof(char);
		CS ncmd = (CS) _alloca(nlen);
		strncpy(ncmd, cmd, i);
		strcpy(ncmd + i, rspstr);
		strcat(ncmd, cmd + j);

		// Replace the old buffer with the new one and
		// continue looking for more response files.
		cmd = ncmd;
		i += nread;
	    }
	}

	// Response files allow (encourage) newlines, so we take a
	// final pass to convert CRLFs to spaces. This may not be
	// strictly necessary but is done for readability.
	for (i = 0; cmd[i]; i++) {
	    if (cmd[i] == '\r' && cmd[i+1] == '\n') {
		cmd[i] = ' ';
		for (j = i + 1; cmd[j]; cmd[j] = cmd[++j]);
	    }
	}
    }

    // Hook the CreateFile{A,W} functions.
    g_CreateFileW = new CAPIHook("kernel32.dll", "CreateFileW",
	(PROC) Hook_CreateFileW, TRUE);
    g_CreateFileA = new CAPIHook("kernel32.dll", "CreateFileA",
	(PROC) Hook_CreateFileA, TRUE);

    // Hook the DeleteFile{A,W} functions.
    g_DeleteFileW = new CAPIHook("kernel32.dll", "DeleteFileW",
	(PROC) Hook_DeleteFileW, TRUE);
    g_DeleteFileA = new CAPIHook("kernel32.dll", "DeleteFileA",
	(PROC) Hook_DeleteFileA, TRUE);

    // Hook the CreateProcess{A,W} functions.
    g_CreateProcessW = new CAPIHook("kernel32.dll",
	"CreateProcessW", (PROC) Hook_CreateProcessW, TRUE);
    g_CreateProcessA = new CAPIHook("kernel32.dll",
	"CreateProcessA", (PROC) Hook_CreateProcessA, TRUE);

    // Hook the CreateProcessAsUser{A,W} functions.
    g_CreateProcessAsUserW = new CAPIHook("kernel32.dll",
	"CreateProcessAsUserW", (PROC) Hook_CreateProcessAsUserW, TRUE);
    g_CreateProcessAsUserA = new CAPIHook("kernel32.dll",
	"CreateProcessAsUserA", (PROC) Hook_CreateProcessAsUserA, TRUE);

    // Hook the CreateProcessWithLogonW function.
    g_CreateProcessWithLogonW = new CAPIHook("kernel32.dll",
	"CreateProcessWithLogonW", (PROC) Hook_CreateProcessWithLogonW, TRUE);

    // Hook the CreateProcessWithTokenW function.
    g_CreateProcessWithTokenW = new CAPIHook("kernel32.dll",
	"CreateProcessWithTokenW", (PROC) Hook_CreateProcessWithTokenW, TRUE);

    // Hook the ExitProcess function.
    g_ExitProcess = new CAPIHook("kernel32.dll",
	"ExitProcess", (PROC) Hook_ExitProcess, TRUE);

    // Now call through to the common code.
    (void)_init_auditlib("init", szThisProgPath, cmd);
}

///////////////////////////////////////////////////////////////////////////////

BOOL APIENTRY
DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
	// We don't do much here because we have our cool Premain hack.
	// PreMain has none of the DllMain restrictions such as:
	// 1. Can't call LoadLibrary.
	// 2. Can't use functions from any DLL other than kernel32.dll.
	// 3. Can't start and wait for a thread.
	// 4. Can't exit.
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
	// We used to flush the audit here but that caused all sorts of
	// problems, presumably due to uncertain order of DLL unloading
	// caused by WSACleanup() here and in the main program or
	// other DLL's. So we moved everything to an ExitProcess hook.
    }

    return TRUE;
}

/****************************************************************************
* Everything from here down is derived from Win32 LD_PRELOAD at
* http://www.deez.info/sengelha/code/win32-ldpreload/
* There has been some significant modification here, particularly
* to add the PreMain capability.
***************************************************************************/

/*---------------------------------------------------------------------------
ldpreload.c
Copyright (c) 2001 Steven Engelhardt
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
notice, this list of conditions, and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions, and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------*/

/*!
* Win32 LD_PRELOAD.
*
* A simple tool to emulate UNIX's LD_PRELOAD functionality.  Works by
* modifying the startup code within the program to first load
* the DLL.  Idea from pg. 794 in _Programming Applications for Microsoft
* Windows_: Fourth Edition by Jeffrey Richter.
*
* This code doesn't exactly correspond to LD_PRELOAD in that it
* will not actually load the specified DLL _before_ all others, but
* simply guarantee that the DLL is loaded before the program begins
* executing.
*
* Works by locating the address at which the process will begin to
* execute, and replacing the code at that address with code which will
* call LoadLibrary() on the specified DLL.  After this is done, the
* original instructions will be restored and execution will continue
* as if nothing ever happened.
*
* This will almost certainly only work on Windows NT 4.0, 2000, and
* up.  It relies on such assumptions as NT EXE headers being present
* and the function VirtualAllocEx existing.  If alternative methods for
* determining the entry point address and finding a block of memory
* within the process to put code exist, these assumptions can probably
* be eliminated and this will work in Windows 95.  It also is limited
* to 32-bit machines because of sizeof(LPVOID) == sizeof(DWORD)
* assumptions.  This also assumes that KERNEL32.DLL will be mapped in
* at the same address in this process and its child.
* 
* This function will allocate a small hunk of memory within the process
* it creates to store the new instructions.  This hunk of memory will
* never be freed (until, of course, the operating system cleans it up).
*/

/// @file
/// @brief Support for DLL injection on Windows.

/// @cond static

/*
* Number of bytes of entry code that we will initially overwrite
*/
#define NUM_ENTRY_CODE_BYTES     7

/*
* Number of bytes of code/data that is used to perform the LD_PRELOAD
* functionality
*/
#define NUM_LDPRELOAD_CODE_BYTES (512 + NUM_ENTRY_CODE_BYTES)

/// @endcond static

/*
* _FindModule
*
* Gets the HMODULE of a process by its ID.
*/
HMODULE
_FindModule(DWORD dwProcessId)
{
    HANDLE hSnapshot;
    MODULEENTRY32 pEntry32;

    pEntry32.dwSize = sizeof(MODULEENTRY32);

    // Get modules snapshot for the given process
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);

    if (hSnapshot == INVALID_HANDLE_VALUE) {
	putil_warn("INVALID_FILE_HANDLE for %d", dwProcessId);
	return NULL;
    }

    if (!Module32First(hSnapshot, &pEntry32)) {
	putil_warn("!Module32First()");
	CloseHandle(hSnapshot);
	return NULL;
    }

    do {
	if (pEntry32.th32ProcessID == dwProcessId) {
	    // Found the process - return its module handle.
	    CloseHandle(hSnapshot);
	    return pEntry32.hModule;
	}
    } while (Module32Next(hSnapshot, &pEntry32));

    CloseHandle(hSnapshot);
    putil_warn("Missing Module");
    return NULL;
}

/*
 * Gets the address of the EXE's entry point: the point at which the EXE
 * will begin executing, by reading image headers.
 */
static BOOL
_GetEntryPointAddrByHeaders(LPPROCESS_INFORMATION pip, const WCHAR *szEXE, LPVOID *pdwEntryAddr)
{
    HMODULE hModule;
    LPVOID lpFileBase = NULL;
    IMAGE_DOS_HEADER dosHeader;
    IMAGE_NT_HEADERS ntHeaders;
    LPVOID ntHeader;
    SIZE_T read;

    // Find the module
    hModule = _FindModule(pip->dwProcessId);
    if (hModule == NULL) {
	putil_warnW(L"%s: Unable to find module", szEXE);
	return FALSE;
    }

    lpFileBase = hModule;

    ReadProcessMemory(pip->hProcess, lpFileBase, &dosHeader, sizeof(IMAGE_DOS_HEADER), &read);
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
	putil_warnW(L"%s: Invalid signature", szEXE);
	return FALSE;
    }

    ntHeader = (LPVOID)((DWORD_PTR)lpFileBase + dosHeader.e_lfanew);
    ReadProcessMemory(pip->hProcess, ntHeader, &ntHeaders, sizeof(ntHeaders), &read);
    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) {
	putil_warnW(L"%s: NT EXE signature not present", szEXE);
	return FALSE;
    }

    if(ntHeaders.FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
	putil_warnW(L"%s: Unsupported machine (0x%x)", szEXE, ntHeaders.FileHeader.Machine);
	return FALSE;
    }

    if(ntHeaders.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
	putil_warnW(L"%s: NT optional header not present", szEXE);
	return FALSE;
    }

    *pdwEntryAddr = (LPVOID)(ntHeaders.OptionalHeader.ImageBase + ntHeaders.OptionalHeader.AddressOfEntryPoint);

    return TRUE;
}

/*
* _WriteEntryCode
*
* Writes the new entry code to the BYTE pointer given.  This entry code
* should be exactly NUM_ENTRY_CODE_BYTES long (an assert will fire
* otherwise).  This code simply jumps to the address given as a parameter
* to this function, which is where the instructions for loading the DLL
* will exist within the process.
*/
static BOOL
_WriteEntryCode(BYTE *pbEntryCode, DWORD_PTR dwLDPreloadInstrAddr)
{
    BYTE *pbEntryCodeCounter = pbEntryCode;

    /* __asm mov eax, dwLDPreloadInstrAddr; */
    *pbEntryCodeCounter++ = 0xB8;
    *((DWORD_PTR *) pbEntryCodeCounter) = (DWORD_PTR) dwLDPreloadInstrAddr;
    pbEntryCodeCounter += sizeof(DWORD_PTR);

    /* __asm jmp eax; */
    *pbEntryCodeCounter++ = 0xFF;
    *pbEntryCodeCounter++ = 0xE0;

    return (pbEntryCodeCounter - pbEntryCode) == NUM_ENTRY_CODE_BYTES;
}

/*
* _WriteLDPreloadCode
*
* Writes the code which will call LoadLibrary and then restore the original
* instructions back to the process entry point, then jumping back to the
* entry point.
*/
static BOOL
_WriteLDPreloadCode(BYTE *pbLDPreloadCode,
		    DWORD_PTR dwLDPreloadInstrAddr,
		    const BYTE *pbOrigEntryCode,
		    LPVOID dwProcessEntryCodeAddr,
		    LPCSTR szDLL,
		    LPCSTR szInitFuncName)
{
    HMODULE hmodKernelDLL;
    FARPROC farprocLoadLibrary;
    FARPROC farprocGetCurrentProcess;
    FARPROC farprocWriteProcessMemory;
    LPVOID pGetProcAddress;
    UINT_PTR dwDataAreaStartAddr;
    DWORD_PTR dwDataAreaDLLStringAddr;	// address for DLL string within process
    size_t nBytesDLLString;
    DWORD_PTR dwDataAreaOrigInstrAddr;	// address for original instructions within process
    size_t nBytesOrigInstr;
    DWORD_PTR dwDataAreaInitFuncStringAddr;	// address for PreMain func
    size_t nBytesInitFuncString = 0;
    const unsigned int k_nMaxInitFuncLength = 8;	// name of PreMain function should not be too long to avoid memory corruption.
    BYTE *pbCurrentArrayPtr;
    const int k_nDataAreaOffsetBytes = 400;	// offset from dwLDPreloadInstrAddr where data area will start

    if (!(hmodKernelDLL = LoadLibrary("kernel32.dll"))) {
	FreeLibrary(hmodKernelDLL);
	return FALSE;
    }

    if (!(farprocLoadLibrary =
	GetProcAddress(hmodKernelDLL, "LoadLibraryA"))) {
	    FreeLibrary(hmodKernelDLL);
	    return FALSE;
    }

    if (!(farprocGetCurrentProcess =
	GetProcAddress(hmodKernelDLL, "GetCurrentProcess"))) {
	    FreeLibrary(hmodKernelDLL);
	    return FALSE;
    }

    if (!(farprocWriteProcessMemory =
	GetProcAddress(hmodKernelDLL, "WriteProcessMemory"))) {
	    FreeLibrary(hmodKernelDLL);
	    return FALSE;
    }

    // How to get proc address of GetProcAddress by using
    // GetProcAddress ? :) i don't know..
    // If we are here this means that GetProcAddress works,
    // so we can't get address without problems.
    pGetProcAddress = &GetProcAddress;

    pbCurrentArrayPtr = pbLDPreloadCode;

    /*
     * Initialize the addresses to the data area members.
     */

    // initialize address of the library name
    dwDataAreaStartAddr = dwLDPreloadInstrAddr + k_nDataAreaOffsetBytes;
    dwDataAreaDLLStringAddr = dwDataAreaStartAddr;
    nBytesDLLString = strlen(szDLL) + 1;

    // initialize address of the original entry point bytes
    dwDataAreaOrigInstrAddr = dwDataAreaDLLStringAddr + nBytesDLLString;
    nBytesOrigInstr = NUM_ENTRY_CODE_BYTES;

    // initialize address of the init function
    dwDataAreaInitFuncStringAddr = dwDataAreaOrigInstrAddr + nBytesOrigInstr;

    if (szInitFuncName) {
	if (strlen(szInitFuncName) < k_nMaxInitFuncLength) {
	    nBytesInitFuncString = strlen(szInitFuncName) + 1;
	} else {
	    putil_warn("%s: init function name (%s) is too long",
		szDLL, szInitFuncName);
	    return FALSE;
	}
    }

    /* Fill with 'int 3' instructions for safety */
    memset(pbCurrentArrayPtr, 0xCC, NUM_LDPRELOAD_CODE_BYTES);

    /*
    * Write the instructions which call LoadLibrary() on szDLL within
    * the process.
    */

    /* __asm mov eax, lpDLLStringStart; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr) = (DWORD_PTR) dwDataAreaDLLStringAddr;
    pbCurrentArrayPtr += sizeof(DWORD_PTR);

    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    /* __asm mov eax, farprocLoadLibrary; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr) = (DWORD_PTR) farprocLoadLibrary;
    pbCurrentArrayPtr += sizeof(DWORD_PTR);

    /* __asm call eax; */
    *pbCurrentArrayPtr++ = 0xFF;
    *pbCurrentArrayPtr++ = 0xD0;

    /*********************************************************************
    Modification by Ivan Moiseev at 10 Jan 2008
    **********************************************************************
    Code above performs LoadLibrary.
    So, now DllMain is finished and all dlls are loaded,
    We will just add code that will call PreMain(void)
    **********************************************************************/

    /* Save module handle that returned by farprocLoadLibrary */
    /* __asm mov ecx, eax; */
    *pbCurrentArrayPtr++ = 0x8B;
    *pbCurrentArrayPtr++ = 0xC8;

    if (szInitFuncName) {

	/* __asm mov eax, dwDataAreaInitFuncStringAddr; */
	*pbCurrentArrayPtr++ = 0xB8;
	*((void **)pbCurrentArrayPtr) =
	    (void *)dwDataAreaInitFuncStringAddr;
	pbCurrentArrayPtr += sizeof(DWORD_PTR);

	/* __asm push eax */
	*pbCurrentArrayPtr++ = 0x50;

	/* __asm push ecx */
	*pbCurrentArrayPtr++ = 0x51;

	/* __asm mov eax, GetProcAddress; */
	*pbCurrentArrayPtr++ = 0xB8;
	*((void **)pbCurrentArrayPtr) = pGetProcAddress;
	pbCurrentArrayPtr += sizeof(DWORD_PTR);

	/* __asm call eax; */
	*pbCurrentArrayPtr++ = 0xFF;
	*pbCurrentArrayPtr++ = 0xD0;

	/* __asm call eax; */
	*pbCurrentArrayPtr++ = 0xFF;
	*pbCurrentArrayPtr++ = 0xD0;
    }

    /*********************************************************************
    End of Modification by Ivan Moiseev
    **********************************************************************/

    /*
    * Write the instructions which will copy the original instructions
    * back to the process's entry point address.  Must use
    * WriteProcessMemory() for security reasons.
    */

    /* pushing arguments to WriteProcessMemory()... */

    // lpNumberOfBytesWritten == NULL
    /* __asm mov eax, 0x0; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr) = (DWORD_PTR) 0x0;
    pbCurrentArrayPtr += sizeof(DWORD_PTR);
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    // nSize == nBytesOrigInstr
    /* __asm mov eax, nBytesOrigInstr; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr) = (DWORD_PTR) nBytesOrigInstr;
    pbCurrentArrayPtr += sizeof(DWORD_PTR);
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    // lpBuffer == dwDataAreaOrigInstrAddr
    /* __asm mov eax, dwDataAreaOrigInstrAddr; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr) = (DWORD_PTR) dwDataAreaOrigInstrAddr;
    pbCurrentArrayPtr += sizeof(DWORD_PTR);
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    // lpBaseAddress == dwProcessEntryCodeAddr
    /* __asm mov eax, dwProcessEntryCodeAddr; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr) = (DWORD_PTR) dwProcessEntryCodeAddr;
    pbCurrentArrayPtr += sizeof(DWORD_PTR);
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    // GetCurrentProcess()
    /* __asm mov eax, farprocGetCurrentProcess; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr) = (DWORD_PTR) farprocGetCurrentProcess;
    pbCurrentArrayPtr += sizeof(DWORD_PTR);

    /* __asm call eax; */
    *pbCurrentArrayPtr++ = 0xFF;
    *pbCurrentArrayPtr++ = 0xD0;

    // hProcess == GetCurrentProcess() == eax
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    /* Done pushing arguments, call WriteProcessMemory() */

    /* __asm mov eax, farprocWriteProcessMemory; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr) = (DWORD_PTR) farprocWriteProcessMemory;
    pbCurrentArrayPtr += sizeof(DWORD_PTR);

    /* __asm call eax; */
    *pbCurrentArrayPtr++ = 0xFF;
    *pbCurrentArrayPtr++ = 0xD0;

    /* Jump back to the processes's original entry point address */

    /* __asm mov eax, dwProcessEntryCodeAddr; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr) = (DWORD_PTR) dwProcessEntryCodeAddr;
    pbCurrentArrayPtr += sizeof(DWORD_PTR);

    /* __asm jmp eax; */
    *pbCurrentArrayPtr++ = 0xFF;
    *pbCurrentArrayPtr++ = 0xE0;

    /*
    * Initialize the 'data area' within the process.
    */

    pbCurrentArrayPtr = pbLDPreloadCode + k_nDataAreaOffsetBytes;

    /* szDLL string */
    memcpy(pbCurrentArrayPtr, szDLL, nBytesDLLString);
    pbCurrentArrayPtr += nBytesDLLString;

    /* origInstr */
    memcpy(pbCurrentArrayPtr, pbOrigEntryCode, nBytesOrigInstr);
    pbCurrentArrayPtr += nBytesOrigInstr;

    /* InitFunc name */
    if (szInitFuncName) {
	memcpy(pbCurrentArrayPtr, szInitFuncName, nBytesInitFuncString);
	pbCurrentArrayPtr += nBytesInitFuncString;
    }

    return TRUE;
}

// Used only by isManagedCode() and could potentially be folded into it.
static DWORD
GetActualAddressFromRVA(IMAGE_SECTION_HEADER* pSectionHeader,
    IMAGE_NT_HEADERS* pNTHeaders, DWORD dwRVA)
{
    DWORD dwRet = 0;
    int j;
    DWORD cbMaxOnDisk;
    DWORD startSectRVA,endSectRVA;

    for(j = 0; j < pNTHeaders->FileHeader.NumberOfSections;
	    j++, pSectionHeader++) {
	cbMaxOnDisk = min( pSectionHeader->Misc.VirtualSize,
	    pSectionHeader->SizeOfRawData );

	startSectRVA = pSectionHeader->VirtualAddress;
	endSectRVA = startSectRVA + cbMaxOnDisk;

	if ( (dwRVA >= startSectRVA) && (dwRVA < endSectRVA)) {
	    dwRet =  (pSectionHeader->PointerToRawData ) +
		(dwRVA - startSectRVA);
	    break;
	}
    }

    return dwRet;
}

// Looks into an executable to determine whether it's a traditional
// native binary or a .NET assembly (aka managed code). So this this
// is used only for a workaround on Windows XP and Vista. It seems
// that Windows 7 doesn't need any special handling for managed code.
static BOOL
isManagedCode(LPCWSTR szEXE)
{
    BOOL bIsManaged = FALSE;

    HANDLE hFile = CreateFileW(szEXE, GENERIC_READ,
			      FILE_SHARE_READ, NULL, OPEN_EXISTING,
			      FILE_ATTRIBUTE_NORMAL, NULL);

    if (INVALID_HANDLE_VALUE == hFile) {
	putil_win32errW(0, GetLastError(), szEXE);
	return FALSE;
    }

    HANDLE hOpenFileMapping = CreateFileMapping(hFile, NULL,
						PAGE_READONLY, 0, 0, NULL);

    if (hOpenFileMapping == NULL) {
	CloseHandle(hFile);
	putil_win32errW(0, GetLastError(), szEXE);
	return FALSE;
    }

    // Map the file so it can simply be acted on as a
    // contiguous array of bytes
    BYTE *lpBaseAddress = (BYTE *) MapViewOfFile(hOpenFileMapping,
						 FILE_MAP_READ, 0, 0, 0);

    if (lpBaseAddress == NULL) {
	CloseHandle(hOpenFileMapping);
	CloseHandle(hFile);
	putil_win32errW(0, GetLastError(), szEXE);
	return FALSE;
    }

    CloseHandle(hOpenFileMapping);
    CloseHandle(hFile);

    IMAGE_DOS_HEADER *pDOSHeader = (IMAGE_DOS_HEADER *) lpBaseAddress;

    IMAGE_NT_HEADERS *pNTHeaders = (IMAGE_NT_HEADERS *) ((BYTE *) pDOSHeader +
							 pDOSHeader->e_lfanew);

    IMAGE_SECTION_HEADER *pSectionHeader =
	(IMAGE_SECTION_HEADER *) ((BYTE *) pNTHeaders +
				  sizeof(IMAGE_NT_HEADERS));

    if (pNTHeaders->Signature == IMAGE_NT_SIGNATURE) {
	//start parsing COM table (this is what points to
	//the metadata and other information)
	DWORD dwNETHeaderTableLocation =
	    pNTHeaders->OptionalHeader.DataDirectory
	    [IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;

	if (dwNETHeaderTableLocation) {
	    // .NET header data does exist for this module;
	    // find its location in one of the sections
	    IMAGE_COR20_HEADER *pNETHeader =
		(IMAGE_COR20_HEADER *) ((BYTE *) pDOSHeader +
		    GetActualAddressFromRVA(pSectionHeader,
			pNTHeaders,
			dwNETHeaderTableLocation));

	    if (pNETHeader) {
		// Valid address obtained. Suffice it to say,
		// this is good enough to identify this as a
		// valid managed component
		bIsManaged = TRUE;
	    }
	}
    }

    UnmapViewOfFile(lpBaseAddress);

    return bIsManaged;
}

/*
 * Does all the magic!
 */
LIBAO_API BOOL
LDPInjectW(LPPROCESS_INFORMATION pip, LPCWSTR szEXE, LPCSTR szDLL, LPCSTR szInitFuncName)
{
    WCHAR szFullEXEPathW[MAX_PATH];
    char szProfilerEV[32];
    LPVOID dwEntryAddr;
    LPVOID lpLDPreloadInstrStorage;
    BYTE rgbOrigEntryCode[NUM_ENTRY_CODE_BYTES];
    BYTE rgbEntryCode[NUM_ENTRY_CODE_BYTES];
    BYTE rgbLDPreloadCode[NUM_LDPRELOAD_CODE_BYTES];

    // Figure out the path to the exe associated with the process.
    if (GetFileAttributesW(szEXE) == INVALID_FILE_ATTRIBUTES) {
	if (!SearchPathW(NULL, szEXE, L".exe",
		charlenW(szFullEXEPathW), szFullEXEPathW, NULL)) {
	    return FALSE;
	}
    } else {
	wcscpy(szFullEXEPathW, szEXE);
    }

    // This is a workaround for a problem specific to managed code
    // on Windows XP and Vista. For some reason I don't understand
    // our regular injection technique doesn't work for managed code
    // on XP/Vista though it does on Windows 7 (and hopefully later).
    // The workaround is to register a COM DLL as a profiler for
    // the managed .exe. This DLL in turn simply loads the auditor
    // DLL and calls its PreMain. Thus, having loaded the profiler,
    // we can return immediately.
    if (GetEnvironmentVariable("COR_ENABLE_PROFILING",
		szProfilerEV, charlen(szProfilerEV))
	    && strtoul(szProfilerEV, NULL, 10)
	    && isManagedCode(szFullEXEPathW)) {
	LPCWSTR cpci_path = L"CPCI.dll";

	vb_printfW(VB_OFF, L"ATTEMPTING CPCI INJECTION FOR %s", szFullEXEPathW);

	HMODULE hATLLib = LoadLibraryW(cpci_path);
	if(hATLLib == NULL) {
	    putil_win32errW(2, GetLastError(), cpci_path);
	}

	PDLLREGISTERSERVER DllRegisterServer =
	    (PDLLREGISTERSERVER)GetProcAddress(hATLLib, "DllRegisterServer");
	if (DllRegisterServer == NULL || (*DllRegisterServer)() != S_OK) {
	    putil_win32errW(2, GetLastError(), cpci_path);
	    FreeLibrary(hATLLib);
	}

	// To be used in the exit path somewhere, if needed at all.
	DllUnRegisterServer = (PDLLUNREGISTERSERVER)
	    GetProcAddress(hATLLib, "DllUnregisterServer");

	return TRUE;
    }

    /*
     * Cause a thread to run to force image to be loaded.
     * We use the name of some API function which is in kernel32.dll,
     * does as little as possible, and requires no parameters.
     * Kernel32.dll is always in memory (except for managed code, apparently).
     */
    HMODULE hK32 = GetModuleHandle("kernel32");
    LPTHREAD_START_ROUTINE loadThreadAddr =
	(LPTHREAD_START_ROUTINE)GetProcAddress(hK32, "AreFileApisANSI");
    HANDLE hRemoteThread = CreateRemoteThread(pip->hProcess,
	NULL, 0, loadThreadAddr, NULL, 0, NULL);
    WaitForSingleObject(hRemoteThread, INFINITE);
    CloseHandle(hRemoteThread);

    /*
     * Get entry point of process.
     */
    if (!_GetEntryPointAddrByHeaders(pip, szFullEXEPathW, &dwEntryAddr)) {
	if (isManagedCode(szFullEXEPathW)) {
	    putil_warn("Auditing of managed code requires Windows 7+");
	    return FALSE;
	} else {
	    // Note: we used to have 3 methods of finding the entry point
	    // but removed two when _GetEntryPointAddrByHeaders seemed
	    // completely stable. But if ever necessary the others should
	    // be findable in version history. _GetEntryPointAddrByPsApi
	    // was removed on 5/16/2010 and the other one somewhat earlier.
	    return FALSE;
	}
    }

    /* Allocate memory block */
    if (!(lpLDPreloadInstrStorage =
	VirtualAllocEx(pip->hProcess, NULL, 500, MEM_COMMIT,
	PAGE_EXECUTE_READWRITE))) {
	    return FALSE;
    }

    /* Copy original instructions from start addr to one memory block */
    if (!ReadProcessMemory(pip->hProcess,
	dwEntryAddr, rgbOrigEntryCode,
	NUM_ENTRY_CODE_BYTES, NULL)) {
	    return FALSE;
    }

    /*
     * Initialize rgbEntryCode (simple push arguments,
     * jmp to memory block #2 code to entry point).
     */
    if (!_WriteEntryCode(rgbEntryCode, (DWORD_PTR) lpLDPreloadInstrStorage)) {
	return FALSE;
    }

    /* Initialize rgbLDPreloadCode */
    if (!_WriteLDPreloadCode(rgbLDPreloadCode,
	(DWORD_PTR) lpLDPreloadInstrStorage, rgbOrigEntryCode,
	dwEntryAddr,
	szDLL,
	szInitFuncName)) {
	    return FALSE;
    }

    /* Write rgbEntryCode to program */
    if (!WriteProcessMemory(pip->hProcess,
	dwEntryAddr, rgbEntryCode,
	NUM_ENTRY_CODE_BYTES, NULL)) {
	    return FALSE;
    }

    /* Write rgbLDPreloadCode to program */
    if (!WriteProcessMemory(pip->hProcess,
	lpLDPreloadInstrStorage, rgbLDPreloadCode,
	NUM_LDPRELOAD_CODE_BYTES, NULL)) {
	    return FALSE;
    }

    return TRUE;
}
