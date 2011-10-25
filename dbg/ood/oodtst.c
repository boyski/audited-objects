// Solaris: gcc -o uxtstood -g -W -Wall uxtstood.c -ldl
// Linux:   gcc -o uxtstood -g -W -Wall -Wl,--export-dynamic uxtstood.c -ldl

#include <stdio.h>
#include <stdlib.h>

#ifdef	_WIN32
#include <windows.h>
#else	/*_WIN32*/
#include <dlfcn.h>
#endif	/*_WIN32*/

//typedef int (*OODPROC_T)(const char *);

static void
dlfailure(void)
{
#ifdef _WIN32
    LPSTR msg;
    DWORD dwBufferLength;
    DWORD dwLastError = GetLastError();
    DWORD dwFormatFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
	    FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;

    if ((dwBufferLength = FormatMessageA(dwFormatFlags, NULL, dwLastError,
		    0, (LPSTR) &msg, 0, NULL))) {
	msg[dwBufferLength] = '\0';
	fprintf(stderr, "Error: %s", msg);
	LocalFree(msg);
    }
#else
    fprintf(stderr, "Error: %s", dlerror());
#endif	/*_WIN32*/

    exit(2);
}

int
oodfunc_date(const char *tgt)
{
    printf("Using datestamp technique for target '%s'\n", tgt);
    return 0;
}

static void *
get_oodfunc(void)
{
    char *lib;

    lib = getenv("MAKEOODLIB");

    if (lib && *lib) {
	int (*oodptr) (const char *);

#ifdef	_WIN32
	HINSTANCE hinstLib = LoadLibrary(lib);
	if (!hinstLib)
	    dlfailure();

	*(void **)(&oodptr) = GetProcAddress(hinstLib, "oodfunc");
	if (!oodptr)
	    dlfailure();
#else	/*_WIN32*/
	// RTLD_LOCAL doesn't work on OSX 10.3 but GLOBAL is fine ...
	void *dlhandle = dlopen(lib, RTLD_GLOBAL | RTLD_LAZY);
	if (!dlhandle)
	    dlfailure();

	*(void **)(&oodptr) = dlsym(dlhandle, "oodfunc");
	if (!oodptr)
	    dlfailure();
#endif	/*_WIN32*/

	return oodptr;
    } else {
	return &oodfunc_date;
    }
}

int
main(int argc, char *argv[])
{
    static int (*oodfunc) (const char *);
    int rc;

    argc = argc;
    argv = argv;

    // The func pointer is static so we only have to look it up once
    // in a make process.
    if (!oodfunc)
	oodfunc = get_oodfunc();

    rc = oodfunc("foo");
    rc = oodfunc("foo");

    return rc;
}
