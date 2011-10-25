#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <io.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>

#include "first.h"

#include <strsafe.h>
#define MAX_THREADS 3
#define BUF_SIZE 255

typedef struct MyData {
    int val1;
    int val2;
} MYDATA, *PMYDATA;

static DWORD WINAPI MyThread( LPVOID lpParam ) 
{ 
    HANDLE hStdout;
    PMYDATA pData;

    TCHAR msgBuf[BUF_SIZE];
    size_t cchStringSize;
    DWORD dwChars;

    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if( hStdout == INVALID_HANDLE_VALUE )
        return 1;

    // Cast the parameter to the correct data type.

    pData = (PMYDATA)lpParam;

    // Print the parameter values using thread-safe functions.

    StringCchPrintf(msgBuf, BUF_SIZE, TEXT("Parameters = %d, %d\n"), 
        pData->val1, pData->val2); 
    StringCchLength(msgBuf, BUF_SIZE, &cchStringSize);
    WriteConsole(hStdout, msgBuf, cchStringSize, &dwChars, NULL);

    return 0; 
} 

static void
TestThread(void)
{
	PMYDATA pData;
	DWORD dwThreadId[MAX_THREADS];
	HANDLE hThread[MAX_THREADS]; 
	int i;


	// Create MAX_THREADS worker threads.

	for( i=0; i<MAX_THREADS; i++ )
	{
		// Allocate memory for thread data.

		pData = (PMYDATA) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			sizeof(MYDATA));

		if( pData == NULL )
			ExitProcess(2);

		// Generate unique data for each thread.

		pData->val1 = i;
		pData->val2 = i+100;

		hThread[i] = CreateThread( 
			NULL,              // default security attributes
			0,                 // use default stack size  
			MyThread,          // thread function 
			pData,             // argument to thread function 
			0,                 // use default creation flags 
			&dwThreadId[i]);   // returns the thread identifier 


		// Check the return value for success.
		// If failure, close existing thread handles, 
		// free memory allocation, and exit. 

		if (hThread[i] == NULL) 
		{
			for(i=0; i<MAX_THREADS; i++)
			{
				if (hThread[i] != NULL)
				{
					CloseHandle(hThread[i]);
				}
			}
			HeapFree(GetProcessHeap(), 0, pData);
			ExitProcess(i);
		}
	}

	// Wait until all threads have terminated.

	WaitForMultipleObjects(MAX_THREADS, hThread, TRUE, INFINITE);

	// Close all thread handles and free memory allocation.

	for(i=0; i<MAX_THREADS; i++)
	{
		CloseHandle(hThread[i]);
	}
	HeapFree(GetProcessHeap(), 0, pData);
}

FIRST_API void
PreMain(void)
{
	fprintf(stderr, "POST-DLL/PRE-MAIN: '%s'\n", GetCommandLine());
	//TestThread();
}


BOOL APIENTRY DllMain( HANDLE hModule, 
					  DWORD  ul_reason_for_call, 
					  LPVOID lpReserved
					  )
{
	if (ul_reason_for_call != DLL_PROCESS_ATTACH)
		return TRUE;

	// We could call PreMain() from here but it would deadlock
	// if it tried to run a thread or load another DLL.
	// So we want to run it from the main program instead,
	// by having DllMain patch its address back into the
	// program. It will then run *AFTER* DllMain returns.

	fprintf(stderr, "ATTACHING FIRST.DLL TO '%s' ...\n", GetCommandLine());

	//Inject(GetCurrentProcess(), GetCurrentThread(),
	    //"prog2", "first.dll", &PreMain);

	return TRUE;
}

//////////////// Following is derived from ldpreload.c ///////////////////

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

/*
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

/*
 * Number of bytes of entry code that we will initially overwrite
 */
#define NUM_ENTRY_CODE_BYTES     7

/*
 * Number of bytes of code/data that is used to perform the LD_PRELOAD
 * functionality
 */
#define NUM_LDPRELOAD_CODE_BYTES (512 + NUM_ENTRY_CODE_BYTES)

/*
 * GetEntryPointAddr
 *
 * Gets the address of the EXE's entry point: the point at which the EXE
 * will begin executing.
 */
static BOOL
GetEntryPointAddr(const char *szEXE, DWORD_PTR * pdwEntryAddr)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hFileMapping = INVALID_HANDLE_VALUE;
    LPVOID lpFileBase = NULL;
    PIMAGE_DOS_HEADER pDOSHeader;
    PIMAGE_NT_HEADERS pNTHeader;

    *pdwEntryAddr = 0;

    hFile = CreateFile(szEXE, GENERIC_READ, FILE_SHARE_READ, NULL,
	    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
	return FALSE;

    if (!(hFileMapping =
		    CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0,
			    NULL))) {
	CloseHandle(hFile);
	return FALSE;
    }

    if (!(lpFileBase =
		    MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0))) {
	CloseHandle(hFile);
	CloseHandle(hFileMapping);
	return FALSE;
    }

    pDOSHeader = (PIMAGE_DOS_HEADER) lpFileBase;
    pNTHeader = (PIMAGE_NT_HEADERS) ((UINT_PTR) lpFileBase +
	    pDOSHeader->e_lfanew);

    if (pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE) {
	fprintf(stderr, "Error: %s: Invalid signature\n", szEXE);
	CloseHandle(hFile);
	CloseHandle(hFileMapping);
	UnmapViewOfFile(lpFileBase);
	return FALSE;
    }

    if (pNTHeader->Signature != IMAGE_NT_SIGNATURE) {
	fprintf(stderr, "Error: %s: NT EXE signature not present\n",
		szEXE);
	CloseHandle(hFile);
	CloseHandle(hFileMapping);
	UnmapViewOfFile(lpFileBase);
	return FALSE;
    }

    if (pNTHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
	fprintf(stderr, "Error: %s: Unsupported machine (0x%x)\n", szEXE,
		pNTHeader->FileHeader.Machine);
	CloseHandle(hFile);
	CloseHandle(hFileMapping);
	UnmapViewOfFile(lpFileBase);
	return FALSE;
    }

    if (pNTHeader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
	fprintf(stderr, "Error: %s: NT optional header not present\n",
		szEXE);
	CloseHandle(hFile);
	CloseHandle(hFileMapping);
	UnmapViewOfFile(lpFileBase);
	return FALSE;
    }

    *pdwEntryAddr =
	    (pNTHeader->OptionalHeader.ImageBase +
	    pNTHeader->OptionalHeader.AddressOfEntryPoint);

    return TRUE;
}


/*
 * WriteEntryCode
 *
 * Writes the new entry code to the BYTE pointer given.  This entry code
 * should be exactly NUM_ENTRY_CODE_BYTES long (an assert will fire
 * otherwise).  This code simply jumps to the address given as a parameter
 * to this function, which is where the instructions for loading the DLL
 * will exist within the process.
 */
static BOOL
WriteEntryCode(BYTE * pbEntryCode, UINT_PTR dwLDPreloadInstrAddr)
{
    BYTE *pbEntryCodeCounter = pbEntryCode;

    /* __asm mov eax, dwLDPreloadInstrAddr; */
    *pbEntryCodeCounter++ = 0xB8;
    *((DWORD_PTR *) pbEntryCodeCounter)++ = (DWORD_PTR) dwLDPreloadInstrAddr;

    /* __asm jmp eax; */
    *pbEntryCodeCounter++ = 0xFF;
    *pbEntryCodeCounter++ = 0xE0;

    return (pbEntryCodeCounter - pbEntryCode) == NUM_ENTRY_CODE_BYTES;
}


/*
 * WriteLDPreloadCode
 *
 * Writes the code which will call LoadLibrary and then restore the original
 * instructions back to the process entry point, then jumping back to the
 * entry point.
 */
static BOOL
WriteLDPreloadCode(BYTE * pbLDPreloadCode,
	UINT_PTR dwLDPreloadInstrAddr,
	const BYTE * pbOrigEntryCode,
	UINT_PTR dwProcessEntryCodeAddr,
	const char *szDLL,
	void *initFunc)
{
    HMODULE hmodKernelDLL;
    FARPROC farprocLoadLibrary;
    FARPROC farprocGetCurrentProcess;
    FARPROC farprocWriteProcessMemory;
    UINT_PTR dwDataAreaStartAddr;
    DWORD_PTR dwDataAreaDLLStringAddr;	// address for DLL string within process
    size_t nBytesDLLString;
    DWORD_PTR dwDataAreaOrigInstrAddr;	// address for original instructions within process
    size_t nBytesOrigInstr;
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

    pbCurrentArrayPtr = pbLDPreloadCode;

    /*
     * Initialize the addresses to the data area members.
     */
    dwDataAreaStartAddr = dwLDPreloadInstrAddr + k_nDataAreaOffsetBytes;
    dwDataAreaDLLStringAddr = dwDataAreaStartAddr;
    nBytesDLLString = strlen(szDLL) + 1;
    dwDataAreaOrigInstrAddr = dwDataAreaDLLStringAddr + nBytesDLLString;
    nBytesOrigInstr = NUM_ENTRY_CODE_BYTES;

    /* Fill with 'int 3' instructions for safety */
    memset(pbCurrentArrayPtr, 0xCC, NUM_LDPRELOAD_CODE_BYTES);

    /*
     * Write the instructions which call LoadLibrary() on szDLL within
     * the process.
     */

    /* __asm mov eax, lpDLLStringStart; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr)++ = (DWORD_PTR) dwDataAreaDLLStringAddr;

    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    /* __asm mov eax, farprocLoadLibrary; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr)++ = (DWORD_PTR) farprocLoadLibrary;

    /* __asm call eax; */
    *pbCurrentArrayPtr++ = 0xFF;
    *pbCurrentArrayPtr++ = 0xD0;

    /*********************************************************************
	    Modification by Ivan Moiseev at 10 Jan 2008
    **********************************************************************
	    Code above performs LoadLibrary("first.dll")
	    So, now DllMain is finished and all dlls are loaded,
	    We will just add code that will call PreMain(void)
    **********************************************************************/

    if (initFunc) {
	//fprintf(stderr, "WRITING %p() at %p in %s\n",
	    //initFunc, pbCurrentArrayPtr, GetCommandLine());

	/* __asm mov eax, initFunc; */
	*pbCurrentArrayPtr++ = 0xB8;
	*((void **) pbCurrentArrayPtr)++ = initFunc;

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
    *((DWORD_PTR *) pbCurrentArrayPtr)++ = (DWORD_PTR) 0x0;
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    // nSize == nBytesOrigInstr
    /* __asm mov eax, nBytesOrigInstr; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr)++ = (DWORD_PTR) nBytesOrigInstr;
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    // lpBuffer == dwDataAreaOrigInstrAddr
    /* __asm mov eax, dwDataAreaOrigInstrAddr; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr)++ = (DWORD_PTR) dwDataAreaOrigInstrAddr;
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    // lpBaseAddress == dwProcessEntryCodeAddr
    /* __asm mov eax, dwProcessEntryCodeAddr; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr)++ = (DWORD_PTR) dwProcessEntryCodeAddr;
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    // GetCurrentProcess()
    /* __asm mov eax, farprocGetCurrentProcess; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr)++ = (DWORD_PTR) farprocGetCurrentProcess;

    /* __asm call eax; */
    *pbCurrentArrayPtr++ = 0xFF;
    *pbCurrentArrayPtr++ = 0xD0;

    // hProcess == GetCurrentProcess() == eax
    /* __asm push eax */
    *pbCurrentArrayPtr++ = 0x50;

    /* Done pushing arguments, call WriteProcessMemory() */

    /* __asm mov eax, farprocWriteProcessMemory; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr)++ = (DWORD_PTR) farprocWriteProcessMemory;

    /* __asm call eax; */
    *pbCurrentArrayPtr++ = 0xFF;
    *pbCurrentArrayPtr++ = 0xD0;

    /* Jump back to the processes's original entry point address */

    /* __asm mov eax, dwProcessEntryCodeAddr; */
    *pbCurrentArrayPtr++ = 0xB8;
    *((DWORD_PTR *) pbCurrentArrayPtr)++ = (DWORD_PTR) dwProcessEntryCodeAddr;

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

    return TRUE;
}

/*
 * Does all the magic!
 */
FIRST_API BOOL
Inject(HANDLE hProcess, HANDLE hThread,
    LPCSTR szEXE, LPCSTR szDLL, void *initFunc)
{
    char szFullEXEPath[MAX_PATH];
    UINT_PTR dwEntryAddr;
    LPVOID lpLDPreloadInstrStorage;
    BYTE rgbOrigEntryCode[NUM_ENTRY_CODE_BYTES];
    BYTE rgbEntryCode[NUM_ENTRY_CODE_BYTES];
    BYTE rgbLDPreloadCode[NUM_LDPRELOAD_CODE_BYTES];

    //fprintf(stderr, "==%s==\n", szEXE);

    // Figure out the path to the exe associated with the process.
    if (GetFileAttributes(szEXE) == INVALID_FILE_ATTRIBUTES) {
	if (!SearchPath(NULL, szEXE, ".exe",
			sizeof(szFullEXEPath), szFullEXEPath, NULL))
	    return FALSE;
    } else {
	strcpy(szFullEXEPath, szEXE);
    }

    /* Get entry point of process */
    if (!GetEntryPointAddr(szFullEXEPath, &dwEntryAddr))
	return FALSE;

    /* Allocate memory block */
    if (!(lpLDPreloadInstrStorage =
		    VirtualAllocEx(hProcess, NULL, 500, MEM_COMMIT,
			    PAGE_EXECUTE_READWRITE)))
	return FALSE;

    /* Copy original instructions from start addr to one memory block */
    if (!ReadProcessMemory(hProcess,
		    (LPVOID) dwEntryAddr, rgbOrigEntryCode,
		    NUM_ENTRY_CODE_BYTES, NULL))
	return FALSE;

    // Initialize rgbEntryCode (simple push arguments,
    // jmp to memory block #2 code to entry point).
    if (!WriteEntryCode(rgbEntryCode, (UINT_PTR) lpLDPreloadInstrStorage))
	return FALSE;

    /* Initialize rgbLDPreloadCode */
    if (!WriteLDPreloadCode(rgbLDPreloadCode,
		    (DWORD_PTR) lpLDPreloadInstrStorage, rgbOrigEntryCode,
		    dwEntryAddr, szDLL, initFunc))
	return FALSE;

    /* Write rgbEntryCode to program */
    if (!WriteProcessMemory(hProcess,
		    (LPVOID) dwEntryAddr, rgbEntryCode,
		    NUM_ENTRY_CODE_BYTES, NULL))
	return FALSE;

    /* Write rgbLDPreloadCode to program */
    if (!WriteProcessMemory(hProcess,
		    lpLDPreloadInstrStorage, rgbLDPreloadCode,
		    NUM_LDPRELOAD_CODE_BYTES, NULL))
	return FALSE;

    return TRUE;
}
