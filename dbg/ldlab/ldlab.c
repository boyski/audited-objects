/*
 * Build Solaris: gcc -m32 -o lib/ldlab.so -g -W -Wall -fpic -shared -nodefaultlibs -D_REENTRANT -Bdirect -z interpose -z ignore ldlab.c -lgcc -lsocket -lnsl -lm -ldl -lrt -lc
 * or:            gcc -m64 -o lib/64/ldlab.so -g -W -Wall -fpic -shared -nodefaultlibs -D_REENTRANT -Bdirect -z interpose -z ignore ldlab.c -lgcc -lsocket -lnsl -lm -ldl -lrt -lc
 *
 * Additional possibilities include:
 *   -z defs -z text -z combreloc -z lazyload
 *
 * Run:   LD_PRELOAD=./ldlab.so ./hello32
 * or:    LD_PRELOAD_32=./lib/ldlab.so ./hello32
 * or:    LD_PRELOAD_64=./lib/64/ldlab.so ./hello64
 *
 * Build Linux: gcc -m32 -o lib/ldlab.so -g -W -Wall -fpic -shared -D_GNU_SOURCE -D_REENTRANT -z defs -z interpose -z combreloc -z lazy ldlab.c -lm -ldl
 * or:          gcc -m64 -o lib64/ldlab.so -g -W -Wall -fpic -shared -D_GNU_SOURCE -D_REENTRANT -z defs -z interpose -z combreloc -z lazy ldlab.c -lm -ldl
 *
 * Run:   LD_PRELOAD='$LIB/ldlab.so' ./hello{32,64}
 *
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static void ldlab_init(void) __attribute__ ((constructor));	// gcc only
static void ldlab_fini(void) __attribute__ ((destructor));	// gcc only

static void
ldlab_init(void)
{
    write(STDERR_FILENO, "= Starting\n", 11);
}

static void
ldlab_fini(void)
{
    /* On Linux, stderr may be closed at this point */
    write(STDERR_FILENO, "= Ending\n", 11);
}
