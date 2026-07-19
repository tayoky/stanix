#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

void c(void){
	for(;;){
		puts("hello\n");
	}
}

int main(){
	pid_t child = fork();
	if(!child)c();
	kill(child,SIGSTOP);
	getchar();
	kill(child,SIGCONT);
	sleep(1);
	kill(child,SIGKILL);
	return 0;
}