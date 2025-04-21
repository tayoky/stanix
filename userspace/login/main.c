#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(int argc,char **argv){
	//setup an env
	putenv("USER=root");
	putenv("HOME=/");
	putenv("SHELL=/bin/tsh");
	printf("starting login\n");

	//print /motd
	pid_t child = fork();
	if(!child){
		const char *arg[] = {
			getenv("SHELL"),
			"-c",
			"cat /motd",
			NULL
		};
		execv(getenv("SHELL"),arg);
		perror(getenv("SHELL"));
		return 1;
	}
	if(child > 0){
		waitpid(child,NULL,0);
	}

	const char *arg[] = {
		getenv("SHELL"),
		NULL
	};
	execv(getenv("SHELL"),arg);
	perror(getenv("SHELL"));
	
	return 0;
}
