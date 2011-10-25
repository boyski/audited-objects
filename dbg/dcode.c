/*
 *    To compile (on Unix):
 *
 *    (Unix) gcc -o hashfiles -O3 -W -Wall hashfiles.c -lz
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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zlib.h"

////////// MurmurHash2 code ////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby

// Note - This code makes a few assumptions about how your machine behaves -

// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4

// And it has a few limitations -

// 1. It will not work incrementally.
// 2. It will not produce the same results on little-endian and big-endian
//    machines.

unsigned int MurmurHash2 ( const void * key, int len, unsigned int seed )
{
	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.

	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value

	unsigned int h = seed ^ len;

	// Mix 4 bytes at a time into the hash

	const unsigned char * data = (const unsigned char *)key;

	while(len >= 4)
	{
		unsigned int k = *(unsigned int *)data;

		k *= m; 
		k ^= k >> r; 
		k *= m; 
		
		h *= m; 
		h ^= k;

		data += 4;
		len -= 4;
	}
	
	// Handle the last few bytes of the input array

	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
	        h *= m;
	};

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
} 

//-----------------------------------------------------------------------------
// MurmurHashAligned2, by Austin Appleby

// Same algorithm as MurmurHash2, but only does aligned reads - should be safer
// on certain platforms. 

// Performance will be lower than MurmurHash2

#define MIX(h,k,m) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }

unsigned int MurmurHashAligned2 ( const void * key, int len, unsigned int seed )
{
	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	const unsigned char * data = (const unsigned char *)key;

	unsigned int h = seed ^ len;

	int align = (int)data & 3;

	if(align && (len >= 4))
	{
		// Pre-load the temp registers

		unsigned int t = 0, d = 0;

		switch(align)
		{
			case 1: t |= data[2] << 16;
			case 2: t |= data[1] << 8;
			case 3: t |= data[0];
		}

		t <<= (8 * align);

		data += 4-align;
		len -= 4-align;

		int sl = 8 * (4-align);
		int sr = 8 * align;

		// Mix

		while(len >= 4)
		{
			d = *(unsigned int *)data;
			t = (t >> sr) | (d << sl);

			unsigned int k = t;

			MIX(h,k,m);

			t = d;

			data += 4;
			len -= 4;
		}

		// Handle leftover data in temp registers

		d = 0;

		if(len >= align)
		{
			switch(align)
			{
			case 3: d |= data[2] << 16;
			case 2: d |= data[1] << 8;
			case 1: d |= data[0];
			}

			unsigned int k = (t >> sr) | (d << sl);
			MIX(h,k,m);

			data += align;
			len -= align;

			//----------
			// Handle tail bytes

			switch(len)
			{
			case 3: h ^= data[2] << 16;
			case 2: h ^= data[1] << 8;
			case 1: h ^= data[0];
					h *= m;
			};
		}
		else
		{
			switch(len)
			{
			case 3: d |= data[2] << 16;
			case 2: d |= data[1] << 8;
			case 1: d |= data[0];
			case 0: h ^= (t >> sr) | (d << sl);
					h *= m;
			}
		}

		h ^= h >> 13;
		h *= m;
		h ^= h >> 15;

		return h;
	}
	else
	{
		while(len >= 4)
		{
			unsigned int k = *(unsigned int *)data;

			MIX(h,k,m);

			data += 4;
			len -= 4;
		}

		//----------
		// Handle tail bytes

		switch(len)
		{
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0];
				h *= m;
		};

		h ^= h >> 13;
		h *= m;
		h ^= h >> 15;

		return h;
	}
}

//-----------------------------------------------------------------------------
// MurmurHashNeutral2, by Austin Appleby

// Same as MurmurHash2, but endian- and alignment-neutral.
// Half the speed though, alas.

unsigned int MurmurHashNeutral2 ( const void * key, int len, unsigned int seed )
{
	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	unsigned int h = seed ^ len;

	const unsigned char * data = (const unsigned char *)key;

	while(len >= 4)
	{
		unsigned int k;

		k  = data[0];
		k |= data[1] << 8;
		k |= data[2] << 16;
		k |= data[3] << 24;

		k *= m; 
		k ^= k >> r; 
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}
	
	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
	        h *= m;
	};

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
} 

////////// SuperFastHash code ////////////////////////////////////////////

#define SEED ( 0 )

#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) || defined(_MSC_VER) || defined (__BORLANDC__)
#define get16bits(d) ( *( (const unsigned short *)( d ) ) )
#endif

#if !defined(get16bits)
#define get16bits(d) ((((const unsigned char *)d)[1] << 8)\
                      +((const unsigned char *)d)[0])
#endif

unsigned int
SuperFastHash( const unsigned char *data, size_t size )
{
	if ( !size || !data )
		return ( 0 );
	
	unsigned int hash = (unsigned int )size + SEED, tmp;
	size_t remainder = ( size & 3 );
	size >>= ( 2 );

	for ( ; size > 0; size-- ) {
		hash += get16bits( data );
		tmp = ( ( get16bits( data + 2 ) << 11 ) ^ hash );
		hash = ( ( hash << 16 ) ^ tmp );
		data += ( 2 * sizeof( unsigned short ) );
		hash += ( hash >> 11 );
	}

	switch ( remainder ) {
		case 3: {
			hash += ( get16bits ( data ) );
			hash ^= ( hash << 16 );
			hash ^= ( data[ sizeof( unsigned short ) ] << 18 );
			hash += ( hash >> 11 );
		} break;
		
		case 2: {
			hash += ( get16bits ( data ) );
			hash ^= ( hash << 11 );
			hash += ( hash >> 17 );
		} break;
		
		case 1: {
			hash += ( *data );
			hash ^= ( hash << 10 );
			hash += ( hash >> 1 );
		} break;
	}

	hash ^= ( hash << 3 );
	hash += ( hash >> 5 );
	hash ^= ( hash << 4 );
	hash += ( hash >> 17 );
	hash ^= ( hash << 25 );
	hash += ( hash >> 6 );

	return ( hash );
}

static void
usage(char *prog)
{
    fprintf(stderr, "Usage: %s <number> CRC32|Murmur|Aligned|Neutral|Super file ...\n",
	prog);
    exit(2);
}

typedef struct {
    const char		*fm_path;
    unsigned char	*fm_fdata;
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
    fmp->fm_fdata = mmap(0, fmp->fm_fsize, prot, MAP_PRIVATE, fmp->fm_fd, 0);
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

const char *
format_to_radix(unsigned radix, char *buf, size_t len, uint32_t val)
{
    char *p;
    int d;

/// @cond code
#define CHRS "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
/// @endcond code

    assert(radix < sizeof(CHRS));
    memset(buf, '0', len);
    p = buf + len;
    *--p = '\0';
    do {
	assert(p >= buf);
	d = val % radix;
	*--p = CHRS[d];
	val /= radix;
    } while (val > 0);

    if (p > buf)
	strcpy(buf, p);

    return buf;
}

int
main(int argc, char *argv[])
{
    int i;
    clock_t start, finish;
    double duration;
    char *path;
    filemap_s fmap;
    unsigned long hcode;
    char *which;
    char hexbuf[256];

    if (argc < 3)
	usage(argv[0]);

    which = argv[1];

    start = clock();

    for (i = 2; i < argc; i++) {
	path = argv[i];

	// Map the file into memory.
	if (!util_map_file(path, &fmap, 1)) {
	    continue;
	}

	switch (*which) {
	    case 'C': case 'c':
		hcode = crc32(0L, Z_NULL, 0);
		hcode = crc32(hcode, fmap.fm_fdata, fmap.fm_fsize);
		break;
	    case 'M': case 'm':
		hcode = MurmurHash2(fmap.fm_fdata, fmap.fm_fsize, 0);
		break;
	    case 'A': case 'a':
		hcode = MurmurHashAligned2(fmap.fm_fdata, fmap.fm_fsize, 0);
		break;
	    case 'N': case 'n':
		hcode = MurmurHashNeutral2(fmap.fm_fdata, fmap.fm_fsize, 0);
		break;
	    case 'S': case 's':
		hcode = SuperFastHash(fmap.fm_fdata, fmap.fm_fsize);
		break;
	    default:
		hcode = 0;
		usage(argv[0]);
		break;
	}

	util_unmap_file(&fmap);

	(void)format_to_radix(36, hexbuf, sizeof(hexbuf), hcode);

	fprintf(stderr, "%s: %s: %s\n", which, hexbuf, path);
    }

    finish = clock();

    duration = (double) (finish - start) / CLOCKS_PER_SEC;
    //fprintf(stderr, "Elapsed time: %2.1f seconds\n", duration);

    return 0;
}
