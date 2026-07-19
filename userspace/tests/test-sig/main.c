#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

volatile int flag = 0;

void handler(){
	printf("SIGUSR1\n");
	flag = 1;
	return;
}

int main(){
	if(signal(SIGUSR1,handler) == SIG_ERR){
		perror("signal");
		return 1;
	}

	//first just test sending the signal
	flag = 0;
	if(raise(SIGUSR1) < 0){
		perror("raise");
		return 1;
	}
	
	//spin until flags is set
	if(!flag){
		printf("raise and signal test failed\n");
		return 1;
	}
	printf("raise and signal test succed\n");

	sigset_t usr1;
	sigemptyset(&usr1);
	sigaddset(&usr1,SIGUSR1);
	if(sigprocmask(SIG_BLOCK,&usr1,NULL) < 0){
		perror("sigprocmask");
		return 1;
	}

	flag = 0;

	if(raise(SIGUSR1) < 0){
		perror("raise");
		return 1;
	}

	//now unblock and the signal should be triggerd
	if(sigprocmask(SIG_UNBLOCK,&usr1,NULL) < 0){
		perror("sigprocmask");
		return 1;
	}
	//spin until flags is set
	if(!flag){
		printf("rblock/unblock test failed\n");
		return 1;
	}
	printf("block/unblock test succed\n");

	return 0;
}