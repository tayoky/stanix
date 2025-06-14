#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
//#include <assert.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/module.h>
#include <ctype.h>
#include <input.h>
#include <dirent.h>
#include <errno.h>
//stanix specific input device header
#include <input.h>

extern char **environ;

int main(int argc,char **argv){
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
	putenv("PATH=/;/bin");
	putenv("USER=root");

	//env test
	printf("environ dump :\n");
	int i = 0;
	while(environ[i]){
		printf("	%s\n",environ[i]);
		i++;
	}

	//launch tsh in the startup script
	const char *arg[] = {
		"/bin/tsh",
		"/startup.sh",
		NULL
	};

	execvp("/bin/tsh",arg);

	perror("/bin/tsh");
	printf("make sure tsh in installed then reboot the system\n");
	return 1;
}
