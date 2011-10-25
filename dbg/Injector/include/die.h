#ifndef DIE_H
#define DIE_H

static void
ErrorExit(LPCSTR file, int line, LPCSTR lpszFunction, LPCSTR arg)
{
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError();

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	    FORMAT_MESSAGE_FROM_SYSTEM |
	    FORMAT_MESSAGE_IGNORE_INSERTS,
	    NULL,
	    dw,
	    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	    (LPTSTR) & lpMsgBuf, 0, NULL);

    fprintf(stderr,
	    "Error: %s:%d %s(%s) == %d: %s",
	    file, line, lpszFunction, arg, dw, lpMsgBuf);

    LocalFree(lpMsgBuf);
    ExitProcess(dw);
}

#define die(func, arg)	ErrorExit(__FILE__, __LINE__, func, arg)

#endif DIE_H
