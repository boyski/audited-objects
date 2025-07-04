#include <stdio.h>
#include <stdlib.h>

#ifndef MESSAGE
#define MESSAGE		"Hello, World"
#endif

void goodbye(void);

int main(void)
{
    printf(MESSAGE);
    fputc('\n', stdout);
    goodbye();
    return 0;
}
