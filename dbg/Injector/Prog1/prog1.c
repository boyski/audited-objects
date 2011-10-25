#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include "die.h"
#include "first.h"

int
main(int argc, char *argv[])
{
	HINSTANCE first_handle = NULL;
	INJECTOR injector;
	PREMAIN premain;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	DWORD wfso;
	LPSTR prog = "prog2";

	fprintf(stderr, "STARTED %s ...\n", GetCommandLine());

	first_handle = LoadLibrary("First");
	if (first_handle == NULL) {
		die("LoadLibrary", "First");
	}

	injector = (INJECTOR) GetProcAddress(first_handle, "Inject");
	if (injector == NULL) {
		die("GetProcAddress", "Inject");
	}

	premain = (PREMAIN) GetProcAddress(first_handle, "PreMain");
	if (premain == NULL) {
		die("GetProcAddress", "PreMain");
	}

	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	if (CreateProcess(NULL, prog, NULL, NULL, FALSE,
		CREATE_SUSPENDED, NULL, NULL, &si, &pi) == 0) {
			die("CreateProcess", prog);
		}

	if (!injector(pi.hProcess, pi.hThread, prog, "First.dll", premain)) {
		TerminateProcess(pi.hProcess, 42);
		die("inject First", prog);
	}

	if (ResumeThread(pi.hThread) ==	-1)	{
		TerminateProcess(pi.hProcess, 42);
		die("ResumeThread", prog);
	}

	wfso = WaitForSingleObject(pi.hProcess,	INFINITE);
	fprintf(stderr,	"EXITED %s ...\n", prog);

	FreeLibrary(first_handle);

	return 0;
}
