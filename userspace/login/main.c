#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc,char **argv){
	for (int i = 1; i < argc; i++){
		if(!strcmp(argv[1],"--setup-stdin-from-stdout")){
			dup2(STDOUT_FILENO,STDIN_FILENO);
		}
	}
	dup2(STDOUT_FILENO,STDERR_FILENO);
	//setup an env
	putenv("USER=root");
	putenv("HOME=/");
	putenv("SHELL=/bin/tsh");
	printf("starting login\n");

	//print /motd
	system("cat /motd");

	const char *arg[] = {
		getenv("SHELL"),
		NULL
	};
	execv(getenv("SHELL"),arg);
	perror(getenv("SHELL"));
	
	return 0;
}
