// gcc -g -W -Wall -I$opsdir/include -o /tmp/thash thash.c -L$opsdir/lib -lkaz

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hash.h"

struct thing {
    const char 		*ca_line;
    unsigned long	 ca_phash;
    unsigned long	 ca_hash;
} thing;

typedef struct thing *ca_o;

static ca_o
ca_new(const char *line)
{
    ca_o ca;

    ca = (ca_o)calloc(1, sizeof(*ca));
    ca->ca_line = strdup(line);
    ca->ca_hash = (unsigned long)line[0];
    ca->ca_phash = 0;
    return ca;
}

static int
hash_comp_test(const void *p1, const void *p2)
{
    assert(p1 == p2);
    return 0;
}

static hash_val_t
hash_fun_test(const void *key)
{
    return (hash_val_t) key;
}

int
main(int argc, char *argv[])
{
    hash_t *htab;
    hnode_t *hnp, *hnp1;
    char *line;
    ca_o ca, pca;
    int i;
    unsigned long phash = 0;

    htab = hash_create(argc, (hash_comp_t) hash_comp_test, hash_fun_test);

    //printf("key width = %lu\n", HASH_VAL_T_MAX);

    for (i = 1; i < argc; i++) {
	line = argv[i];
	ca = ca_new(line);
	ca->ca_phash = phash;
	phash = ca->ca_hash;

	if (! hash_lookup(htab, (void *)ca->ca_hash)) {
	    hnp = hnode_create(ca);
	    hash_insert(htab, hnp, (void *)ca->ca_hash);
	    printf("%s [%lu.%lu]\n", ca->ca_line, ca->ca_phash, ca->ca_hash);
	} else {
	    printf("%s present\n", line);
	}
    }

    hscan_t hscan;
    hash_scan_begin(&hscan, htab);
    for (hnp = hash_scan_next(&hscan); hnp; hnp = hash_scan_next(&hscan)) {
	ca = (ca_o)hnode_get(hnp);
	printf("%s [%lu.%lu]\n", ca->ca_line, ca->ca_phash, ca->ca_hash);
	if ((hnp1 = hash_lookup(htab, (void *)ca->ca_phash))) {
	    pca = (ca_o)hnode_get(hnp1);
	    printf("parent of %s is %s\n", ca->ca_line, pca->ca_line);
	} else {
	    printf("%s [%lu.%lu] has no parent\n",
		ca->ca_line, ca->ca_phash, ca->ca_hash);
	}
	//hash_scan_delete(htab, hnp);
	//hnode_destroy(hnp);
    }

    return 0;
}
