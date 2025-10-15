#include <stdio.h>
#include <pthread.h>
#include <stddef.h>

volatile int flag = 0;

void *test(void *arg){
	printf("hello from thread %ld\n",pthread_self());
	flag = 1;
}

int main(){
	pthread_t thread;
	pthread_create(&thread,NULL,test,NULL);
	while(!flag);
	return 0;
}