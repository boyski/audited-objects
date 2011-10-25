#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include "die.h"

int
main(int argc, char *argv[])
{
    fprintf(stderr, "STARTED %s ...\n", argv[0]);

    return 0;
}
