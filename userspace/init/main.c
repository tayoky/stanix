#include <fcntl.h>
#include <syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <syscall.h>

void __tlibc_init();

int main(){
	__tlibc_init();

	//init std streams
	open("dev:/null",O_RDONLY); //stdin
	open("dev:/tty0",O_WRONLY); //stdout
	open("dev:/tty0",O_WRONLY); //stderr

	printf("hello world !!\n");
	int fd = open("tmp:/test.txt",O_CREAT |O_RDWR);

	struct timeval time;
	sleep(2);
	gettimeofday(&time,NULL);
	printf("unix timestamp : %ld\n",time.tv_sec);

	//pipe test
	int pipefd[2];
	pipe(pipefd);
	FILE *pipe_read  = fdopen(pipefd[0],"r");
	FILE *pipe_write = fdopen(pipefd[1],"w");
	fprintf(pipe_write,"hello from pipe !!!\n");

	//read from pipe
	printf("read pipe content : \n");

	while(!feof(pipe_read)){
		putchar(getc(pipe_read));
	}
	
	
	//assert(5 == 3);

	return 0;
}

