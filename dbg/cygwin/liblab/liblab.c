#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(__CYGWIN__)

#include <windows.h>
#include <sys/cygwin.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpvReserved;
    cygwin_internal (CW_HOOK, "puts", puts);
    return TRUE;
}

#endif

int
puts(const char *s) {
    printf("Hi%s", s);
    return 0;
}

/* TBD */
int
XXopen(const char *path, int oflag, ...)
{
    (void)path;
    (void)oflag;
    return 0;
}
