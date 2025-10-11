#include <stdio.h>
#include <pthread.h>
#include <stddef.h>

void *test(void *arg){
	puts("hello\n");
}

int main(){
	pthread_t thread;
	pthread_create(&thread,NULL,test,NULL);
}