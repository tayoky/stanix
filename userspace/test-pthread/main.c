#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t mutex;

void *test(void *arg) {
	printf("hello from thread %ld\n", gettid());
	printf("got %p as arg\n", arg);

	pthread_mutex_lock(&mutex);
	puts("got mutex");
	pthread_mutex_unlock(&mutex);
	return (void*)0x4242;
}

int main() {
	pthread_mutex_init(&mutex, NULL);

	pthread_mutex_lock(&mutex);

	pthread_t thread;
	pthread_create(&thread, NULL, test, (void*)0x1234);
	sleep(1);
	pthread_mutex_unlock(&mutex);

	void *ret;
	pthread_join(thread, &ret);
	printf("got %p as return value\n", ret);
	return 0;
}
