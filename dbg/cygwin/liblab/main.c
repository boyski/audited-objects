#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int
main(void) {
    int fd;

    puts("Ho\n");
    fd = open("/etc/passwd", O_RDONLY);
    return fd < 0;
}
