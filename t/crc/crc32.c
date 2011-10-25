// Unix:    gcc -O2 -o crc32 -W -Wall crc32.c -lz
// Windows: cl -nologo -D WIN32 -I U:\src\zlib\win2k\zlib-1.2.1 -MD -o crc32 crc32.c U:\src\zlib\win2k\zlib-1.2.1\zlib.lib Imagehlp.lib

#ifdef WIN32
//#define _WIN32_WINNT 0x0500
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Imagehlp.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "zlib.h"

#ifdef WIN32
#define popen		_popen
#define pclose		_pclose
#define snprintf	_snprintf
#endif

#define endof(str)      strchr(str, '\0')

int
main(int argc, char *argv[])
{
    int i;
    int ar_file;
    size_t len;
    unsigned long crc = 0;
    char *path;
    char buf[4096], arbuf[4096];
    FILE *fp;

    for (i = 1; i < argc; i++) {
	crc = crc32(0L, Z_NULL, 0);
	path = argv[i];
	ar_file = !strcmp(endof(path) - 2, ".a");
	if (ar_file) {
	    snprintf(arbuf, sizeof(arbuf), "ar -p %s", path);
	    if ((fp = popen(arbuf, "r"))) {
		while ((len = fread(buf, 1, sizeof(buf), fp))) {
		    crc = crc32(crc, buf, len);
		}
		pclose(fp);
	    }
#ifdef	WIN32
	} else if (!strcmp(endof(path) - 4, ".obj")) {
	    DWORD origsum;
	    DWORD rc;

	    rc = MapFileAndCheckSum(path, &origsum, &crc);
	    if (rc != CHECKSUM_SUCCESS) {
		fprintf(stderr, "Error: %s: %ld\n", path, rc);
	    }
	    fp = stdin;		// HACK
#endif	/*WIN32*/
	} else {
	    if ((fp = fopen(path, "r"))) {
		while ((len = fread(buf, 1, sizeof(buf), fp))) {
		    crc = crc32(crc, buf, len);
		}
		fclose(fp);
	    }
	}
	if (!fp) {
	    fprintf(stderr, "Error: %s: %s\n", path, strerror(errno));
	    continue;
	}
	fprintf(stdout, "Zlib CRC32 of %s is %lu\n", path, crc);
    }
    return(0);
}
