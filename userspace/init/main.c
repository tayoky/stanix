#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/module.h>
#include <ctype.h>
#include <input.h>
#include <dirent.h>
#include <errno.h>

extern char **environ;

int main(){
	//simple security
	if(getpid() != 0){
		fprintf(stderr,"error : not runned as init process\n");
		fprintf(stderr,"running two init process might crash the system\n");
		return 1;
	}

	//init std streams
	open("/dev/null",O_RDONLY); //stdin
	open("/dev/tty0",O_WRONLY); //stdout
	open("/dev/tty0",O_WRONLY); //stderr


	printf("starting stanix userspace ....\n");
	
	struct timeval time;
	gettimeofday(&time,NULL);
	printf("current unix timestamp : %ld\n",time.tv_sec);

	//setup fake env
	putenv("PATH=/:/bin:/usr/bin");
	putenv("USER=root");

	//env test
	printf("environ dump :\n");
	int i = 0;
	while(environ[i]){
		printf("	%s\n",environ[i]);
		i++;
	}

	pid_t child = fork();
	if(!child){
		//launch tsh in the startup script
		static char *arg[] = {
			"tash",
			"/etc/init.d/startup.sh",
			NULL
		};

		execvp("tash",arg);

		perror("tash");
		printf("make sure tash is installed then reboot the system\n");
		return 1;
	}

	//just cleanup oprhan process
	for(;;){
		waitpid(-1,NULL,0);
	}
}
