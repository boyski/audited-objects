#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cdb.h"

int
main(int argc, char *argv[])
{
    struct cdb_make cdbm;
    int fd, i;
    char *key, *val;
    unsigned vlen, vpos;
    char *cdbfile = "trycdb.cdb";
    char *tmpfile = "trycdb.tmp";
    struct cdb cdb;

    argc = argc;

    fd = open(tmpfile, O_RDWR | O_CREAT, 0666);	/* open temporary file */

    cdb_make_start(&cdbm, fd);	/* initialize structure */

    // add as many records as needed
    for (i = 1; argv[i] && argv[i + 1]; i++) {
	key = argv[i];
	val = argv[i+1];
	cdb_make_add(&cdbm, key, strlen(key), val, strlen(val));
    }

    // final stage - write indexes to CDB file
    cdb_make_finish(&cdbm);

    close(fd);

    // atomically replace CDB file with newly built one
    rename(tmpfile, cdbfile);

    fd = open(cdbfile, O_RDONLY);

    cdb_init(&cdb, fd);		/* initialize internal structure */
    for (i = 1; argv[i] && argv[i + 1]; i += 2) {
	key = argv[i];
	if (cdb_find(&cdb, key, strlen(key)) > 0) {
	    vpos = cdb_datapos(&cdb);	/* position of data in a file */
	    vlen = cdb_datalen(&cdb);
	    val = malloc(vlen);
	    cdb_read(&cdb, val, vlen, vpos);
	    val[vlen] = '\0';
	    fprintf(stdout, "%s=%s\n", key, val);
	} else {
	    fprintf(stderr, "No such key: %s\n", key);
	}
    }

    close(fd);

    return 0;
}
