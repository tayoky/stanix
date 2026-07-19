#include <stdio.h>
#include <signal.h>
#include <string.h>

volatile int e = 0;

void handle(int signum){
	printf("recive %s\n",strsignal(signum));
	e = 1;
}

int main(){
	signal(SIGINT,handle);
	while(!e);
	return 0;
}