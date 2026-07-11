#include <stdio.h>
#include <pthread.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>

pthread_mutex_t mutex;

void *test(void *arg){
	(void)arg;
	printf("hello from thread %ld\n", gettid());

	pthread_mutex_lock(&mutex);	
	puts("got mutex");
	pthread_mutex_unlock(&mutex);
	return NULL;
}

int main(){
	pthread_mutex_init(&mutex, NULL);

	pthread_mutex_lock(&mutex);

	pthread_t thread;
	pthread_create(&thread,NULL,test,NULL);
	sleep(1);
	pthread_mutex_unlock(&mutex);
	pthread_join(thread,NULL);
	return 0;
}
