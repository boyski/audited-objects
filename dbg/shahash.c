/*
 *    To compile (on Unix):
 *
 *    gcc -o shahash -O3 -W -Wall shahash.c -lcrypto
 *
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    fprintf(stderr, "Usage: %s file\n", prog);
    exit(2);
}

typedef struct {
    const char		*fm_path;
    char		*fm_fdata;
    size_t		 fm_fsize;
    int			 fm_fd;
} filemap_s;

static filemap_s *
util_map_file(const char *path, filemap_s *fmp)
{
    struct stat stbuf;
    int prot;

    prot = PROT_READ;

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
    fmp->fm_fdata = mmap(0, fmp->fm_fsize, prot, MAP_PRIVATE, fmp->fm_fd, 0);
    if (fmp->fm_fdata == MAP_FAILED) {
	perror(path);
	return NULL;
    }

    close(fmp->fm_fd);

    // A potential optimization.
    if (madvise(fmp->fm_fdata, fmp->fm_fsize, MADV_SEQUENTIAL)) {
	perror(path);
	return NULL;
    }

    fmp->fm_path = path;
    return fmp;
}

int
main(int argc, char *argv[])
{
    char *path;
    filemap_s fmap;
    char hash[256];
    int i;

    if (argc < 2)
	usage(argv[0]);

    for (i = 1; i < argc; i++) {
	SHA_CTX ctx;
	unsigned char sha1[SHA_DIGEST_LENGTH];
	char prefix[64];

	path = argv[i];

	// Map the file into memory.
	if (!util_map_file(path, &fmap)) {
	    return 2;
	}

	SHA1_Init(&ctx);


	snprintf(prefix, sizeof(prefix),
	    "blob %lu", (unsigned long)fmap.fm_fsize);
	SHA1_Update(&ctx, prefix, strlen(prefix) + 1);

	SHA1_Update(&ctx, fmap.fm_fdata, fmap.fm_fsize);
	SHA1_Final(sha1, &ctx);
	sha1_to_hex(sha1, hash);

	if (munmap(fmap.fm_fdata, fmap.fm_fsize) == -1) {
	    perror(fmap.fm_path);
	    return -1;
	}

	puts(hash);
	putc('\n', stdout);
    }

    return 0;
}
