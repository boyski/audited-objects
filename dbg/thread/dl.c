// gcc -Wall -W -o dl -g  dl.c 
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NTHREADS	4

void *
Hello(void *threadid)
{
    int i;
    int fd;
    char buf[256];
    double result = 0.0;
    for (i = 0; i < 10000; i++) {
	result = result + (double)random();
    }
    if (threadid == threadid)
	printf("%d: Hello World!\n", pthread_self());
    snprintf(buf, sizeof(buf), "%d.tst", pthread_self());
    fd = creat(buf, 0666);
    close(fd);
    return NULL;
}

int
main(void)
{
    pthread_t threads[NTHREADS];
    int rc, t;
    for (t = 0; t < NTHREADS; t++) {
	rc = pthread_create(&threads[t], NULL, Hello, (void *)t);
	if (rc) {
	    printf("ERROR: return code from pthread_create() is %d\n", rc);
	    printf("Code %d= %s\n", rc, strerror(rc));
	    exit(-1);
	}
    }
    printf("main(): Created %d threads [%d]\n", t, pthread_self());
    pthread_exit(NULL);
    return 0;
}
