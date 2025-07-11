#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int main(){
	if(getuid() != 0){
		fprintf(stderr,"user must be run as root !\n");
		return 1;
	}
	

	setgid(1);
	setuid(1);

	const char *arg[] = {
		getenv("SHELL"),
		NULL
	};

	execv(getenv("SHELL"),arg);
	perror(getenv("SHELL"));
	
	return 0;
}
