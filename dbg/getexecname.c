// gcc -o getexecname -W -Wall -g getexecname.c
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[]) {
    printf("prog=%s\n", getprogname());	// *BSD, Darwin
    return 0;
}
