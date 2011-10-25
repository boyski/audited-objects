#include <ar.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

int
main(int argc, char *argv[])
{
    char *libname;
    int fd;
    struct stat stbuf;
    void *libdata, *dot;
    struct ar_hdr hdr;

    while ((libname = *++argv)) {
	if ((fd = open(libname, O_RDONLY)) == -1) {
	    perror(libname);
	    exit(2);
	}
	if (fstat(fd, &stbuf)) {
	    perror(libname);
	    exit(2);
	}
	libdata = mmap(0, stbuf.st_size,
	    PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (libdata == MAP_FAILED) {
	    perror(libname);
	    exit(2);
	}
	close(fd);

	if (memcmp(libdata, ARMAG, SARMAG)) {
	    fprintf(stderr, "Warning: not an archive: %s\n", libname);
	    continue;
	}

	for (dot = libdata + SARMAG; dot < libdata + stbuf.st_size; ) {
	    off_t size;
	    char *p;

	    memcpy(&hdr, dot, sizeof(hdr));
	    size = atoi(hdr.ar_size);
	    size += size % 2;
	    dot += sizeof(hdr);
	    if (hdr.ar_name[0] == '/' &&
		    (hdr.ar_name[1] == '\0' ||
		     hdr.ar_name[1] == '/' ||
		     hdr.ar_name[1] == ' ')) {
		dot += size;
		continue;
	    }
	    fprintf(stderr, "name='");
	    for (p = hdr.ar_name; *p != '/'; p++)
		fputc(*p, stderr);
	    fprintf(stderr, "' size=%d\n", size);
	    dot += size;
	}

	if (munmap(libdata, stbuf.st_size) == -1) {
	    perror(libname);
	    exit(2);
	}
    }


    return 0;
}
