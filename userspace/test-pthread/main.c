#include <stdio.h>
#include <pthread.h>
#include <stddef.h>
#include <errno.h>

void *test(void *arg){
	printf("hello from thread %ld\n",pthread_self());
	errno = 2;
}

int main(){
	pthread_t thread;
	errno = 1;
	pthread_create(&thread,NULL,test,NULL);
	pthread_join(thread,NULL);
	return 0;
}
