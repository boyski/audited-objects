#include <stdlib.h>
#include <stdio.h>

#ifndef MESSAGE
#define MESSAGE		"Hello, World"
#endif

int main(void)
{
    printf(MESSAGE);
    fputc('\n', stdout);
    return 0;
}
