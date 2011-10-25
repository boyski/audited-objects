#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


void *
f1(void *arg)
{
    void *handle = NULL;

    if (!handle) {
	handle = dlopen(FEDORA, RTLD_LAZY);
	if (handle) {
	    fprintf(stderr, "dlopen() = %p\n", handle);
	} else {
	    fprintf(stderr, "Error: dlopen: %s\n", dlerror());
	}
	close(200);
	sleep(rand() % (int)arg);
	printf("f1\n");
	if (handle) {
	    fprintf(stderr, "dlclose(%p)\n", handle);
	    dlclose(handle);
	    handle = NULL;
	}
    }
    return NULL;
}

void *
f2(void *arg)
{
    void *handle = NULL;

    if (!handle) {
	handle = dlopen(FEDORA, RTLD_LAZY);
	if (handle) {
	    fprintf(stderr, "dlopen() = %p\n", handle);
	} else {
	    fprintf(stderr, "Error: dlopen: %s\n", dlerror());
	}
	close(200);
	sleep(rand() % (int)arg);
	printf(" f2\n");
	if (handle) {
	    fprintf(stderr, "dlclose(%p)\n", handle);
	    dlclose(handle);
	    handle = NULL;
	}
    }
    return NULL;
}

int
main(int argc, char *argv[])
{
    int num, i;
    pthread_t *tids1, *tids2;
    int delay = 10;

    assert(argc == 2);

    num = atoi(argv[1]);

    tids1 = (pthread_t *)calloc(sizeof(pthread_t), num);
    tids2 = (pthread_t *)calloc(sizeof(pthread_t), num);

    for (i = 0; i < num; i++) {
	if (pthread_create(tids1 + i, 0, f1, (void *)3)) {
	    perror("f1");
	}

	if (pthread_create(tids2 + i, 0, f2, (void *)5)) {
	    perror("f2");
	}

	sleep(1);
    }

    printf("sleeping %d ...\n", delay);
    sleep(delay);
    return 0;
}
