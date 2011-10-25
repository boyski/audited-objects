/*
 *    To compile (on Unix):
 *
 *    (Unix) gcc -o hashfiles -O3 -W -Wall hashfiles.c -lz -lmurmur
 *
 *    (Win32) cl /c /TP /O2 /I ..\ops\%PLATFORM%\include hashfiles.c
 *            link hashfiles.obj /libpath:..\ops\%PLATFORM%\lib zlibMT7.lib
 *
 *    This program links with zlib (www.zlib.org) and thus the compile
 *    line may need to be enhanced with a -I flag pointing to the
 *    location of zlib.h and a -L flag with the location of libz.a
 *
 */

#if defined(_WIN32)
#define _WIN32_WINNT 0x0500
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zlib.h"
#include <openssl/ssl.h>
#include <openssl/evp.h>

const signed char hexval_table[256] = {
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 00-07 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 08-0f */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 10-17 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 18-1f */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 20-27 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 28-2f */
	  0,  1,  2,  3,  4,  5,  6,  7,		/* 30-37 */
	  8,  9, -1, -1, -1, -1, -1, -1,		/* 38-3f */
	 -1, 10, 11, 12, 13, 14, 15, -1,		/* 40-47 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 48-4f */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 50-57 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 58-5f */
	 -1, 10, 11, 12, 13, 14, 15, -1,		/* 60-67 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 68-67 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 70-77 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 78-7f */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 80-87 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 88-8f */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 90-97 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* 98-9f */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* a0-a7 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* a8-af */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* b0-b7 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* b8-bf */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* c0-c7 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* c8-cf */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* d0-d7 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* d8-df */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* e0-e7 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* e8-ef */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* f0-f7 */
	 -1, -1, -1, -1, -1, -1, -1, -1,		/* f8-ff */
};

static inline unsigned int hexval(unsigned char c)
{
    return hexval_table[c];
}

static void
sha1_to_hex(const unsigned char *sha1, char *buf)
{
    static const char hex[] = "0123456789abcdef";
    int i;

    for (i = 0; i < 20; i++) {
	unsigned int val = *sha1++;
	*buf++ = hex[val >> 4];
	*buf++ = hex[val & 0xf];
    }
    *buf = '\0';
}

static void
usage(char *prog)
{
    fprintf(stderr, "Usage: %s CRC32|SHA1 count file\n", prog);
    exit(2);
}

typedef struct {
    const char		*fm_path;
    char		*fm_fdata;
    size_t		 fm_fsize;
#if defined(_WIN32)
    HANDLE		 fm_fh;
#else
    int			 fm_fd;
#endif
} filemap_s;

static filemap_s *
util_map_file(const char *path, filemap_s *fmp, int writeable)
{
#if defined(_WIN32)
    HANDLE hMap;
    DWORD protection;

    protection = writeable ? PAGE_WRITECOPY : PAGE_READONLY;

    fmp->fm_fh = CreateFile(path, GENERIC_READ,
	    FILE_SHARE_READ, NULL, OPEN_EXISTING,
	    FILE_ATTRIBUTE_NORMAL, NULL);
    if (fmp->fm_fh == INVALID_HANDLE_VALUE) {
	perror(path);
	return NULL;
    }
    
    if ((fmp->fm_fsize = GetFileSize(fmp->fm_fh, NULL)) == INVALID_FILE_SIZE) {
	perror(path);
	return NULL;
    }

    if (!(hMap = CreateFileMapping(fmp->fm_fh,
	    NULL, protection, 0, 0, NULL))) {
	perror(path);
	return NULL;
    }

    if (!(fmp->fm_fdata = (unsigned char *)MapViewOfFile(hMap,
	    FILE_MAP_COPY, 0, 0, fmp->fm_fsize))) {
	perror(path);
	return NULL;
    }
#else
    struct stat stbuf;
    int prot;

    prot = writeable ? PROT_READ|PROT_WRITE : PROT_READ;

    if ((fmp->fm_fd = open(path, O_RDONLY)) == -1) {
	perror(path);
	return NULL;
    }

    if (fstat(fmp->fm_fd, &stbuf)) {
	perror(path);
	return NULL;
    }

    fmp->fm_fsize = stbuf.st_size;

    // This could fail if the file is too big for available swap...
    fmp->fm_fdata = mmap64(0, fmp->fm_fsize, prot, MAP_PRIVATE, fmp->fm_fd, 0);
    if (fmp->fm_fdata == MAP_FAILED) {
	perror(path);
	return NULL;
    }

    // A potential optimization.
    if (madvise(fmp->fm_fdata, fmp->fm_fsize, MADV_SEQUENTIAL)) {
	perror(path);
	return NULL;
    }
#endif

    fmp->fm_path = path;
    return fmp;
}

static int
util_unmap_file(filemap_s *fmp)
{
#if defined(_WIN32)
    if (UnmapViewOfFile(fmp->fm_fdata) == 0) {
	perror(fmp->fm_path);
	return -1;
    }

    CloseHandle(fmp->fm_fh);
#else
    if (munmap(fmp->fm_fdata, fmp->fm_fsize) == -1) {
	perror(fmp->fm_path);
	return -1;
    }

    close(fmp->fm_fd);
#endif

    return 0;
}

int
main(int argc, char *argv[])
{
    int i;
    clock_t start, finish;
    double duration;
    char *path;
    filemap_s fmap;
    char hash[256];
    int times;
    char *which;

    which = argv[1];
    times = atoi(argv[2]);
    path = argv[3];

    if (argc != 4 || times <= 0)
	usage(argv[0]);

    // Map the file into memory.
    if (!util_map_file(path, &fmap, 1)) {
	return 2;
    }

    start = clock();

    for (i = 0; i < times; i++) {
	switch (*which) {
	    case 'C': case 'c':
	    {
		unsigned long hcode = 0;

		hcode = crc32(crc32(0L, Z_NULL, 0), (unsigned char *)fmap.fm_fdata, fmap.fm_fsize);
		snprintf(hash, sizeof(hash), "%010lu", hcode);
		break;
	    }
	    case 'S': case 's':
	    case 'G': case 'g':
	    {
		SHA_CTX ctx;
		unsigned char sha1[SHA_DIGEST_LENGTH];

		SHA1_Init(&ctx);

		if (*which == 'G' || *which == 'g') {
		    char prefix[64];

		    snprintf(prefix, sizeof(prefix), "blob %lu", (unsigned long)fmap.fm_fsize);
		    SHA1_Update(&ctx, prefix, strlen(prefix) + 1);
		}

		SHA1_Update(&ctx, fmap.fm_fdata, fmap.fm_fsize);
		SHA1_Final(sha1, &ctx);
		sha1_to_hex(sha1, hash);
		break;
	    }
	    default:
		usage(argv[0]);
		break;
	}
    }

    finish = clock();

    util_unmap_file(&fmap);

    duration = (double) (finish - start) / CLOCKS_PER_SEC;
    fprintf(stderr, "%s, elapsed time: %2.1f seconds\n", hash, duration);

    return 0;
}
