#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void *
f1(void *arg)
{
	(void)arg;
	printf("f1\n");
	(void)dlopen(FEDORA, RTLD_LAZY);
	close(10);
	sleep(3);
	return NULL;
}

void *
f2(void *arg)
{
	(void)arg;
	printf("f2\n");
	(void)dlopen(FEDORA, RTLD_LAZY);
	close(20);
	sleep(3);
	return NULL;
}

int
main(void)
{
	static pthread_t t1, t2;

	if (pthread_create(&t1, 0, f1, 0))
		perror("f1");
	if (pthread_create(&t2, 0, f2, 0))
		perror("f2");
	printf("slp\n");
	sleep(20);
	return 0;
}

