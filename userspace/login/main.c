#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc,char **argv){
	if(getuid() != 0){
		fprintf(stderr,"login must be run as root !\n");
		return 1;
	}
	
	for (int i = 1; i < argc; i++){
		if(!strcmp(argv[1],"--setup-stdin-from-stdout")){
			dup2(STDOUT_FILENO,STDIN_FILENO);
		}
	}
	dup2(STDOUT_FILENO,STDERR_FILENO);
	//setup an env
	putenv("USER=root");
	putenv("HOME=/home");
	putenv("SHELL=/bin/tsh");
	chdir(getenv("HOME"));

	//print /motd
	system("cat /etc/motd");

	const char *arg[] = {
		getenv("SHELL"),
		NULL
	};
	execv(getenv("SHELL"),arg);
	perror(getenv("SHELL"));
	
	return 0;
}
