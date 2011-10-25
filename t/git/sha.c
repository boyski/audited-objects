/* gcc -o sha -DGIT2 -W -Wall -g -I../../OPS/include sha.c -L../../OPS/Linux_x86_64/lib -lgit2 -lz */

/*
 * When compiled as above but without -DGIT2, the result is an executable of ~10K bytes.
 * When -DGIT2 is added the executable grows to 500K bytes or ~50 times larger,
 * primarily due to the use of git_odb_hash() (git_oid_fmt doesn't add much).
 * This is on Ubuntu 11.04 64-bit.
 */

#include <stdio.h>
#include <string.h>
#include "git2/odb.h"

int
main(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
	const char *arg = argv[i];
	char buf[64] = "----------------------------------------";

#ifdef GIT2
	git_oid oid;

	git_odb_hash(&oid, arg, strlen(arg), GIT_OBJ_BLOB);
	git_oid_fmt(buf, &oid);
#endif

	buf[40] = '\0';
	printf("%s: %s\n", arg, buf);
    }

    return 0;
}
