#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    char *msg = getenv("HELLO");

    if (msg)
	printf("%s\n", msg);
    else
	printf("Hello, World\n");
    exit(0);
}

