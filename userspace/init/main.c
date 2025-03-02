#include <fcntl.h>
#include <syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
//#include <assert.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syscall.h>
#include <ctype.h>
#include <input.h>
#include <dirent.h>
#include <errno.h>

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

	//try forking
	pid_t child = fork();
	if(!child){
		printf("i'am the child\n");
		exit(0);
	}
	printf("i'am the parent\n");

	//try do some ls
	DIR *dir = opendir("dev:/");
	if(!dir){
		fprintf(stderr,"%s\n",strerror(errno));
		exit(1);
	}
	for(;;){
		struct dirent *ret = readdir(dir);
		if(!ret){
			break;
		}
		printf("%s\n",ret->d_name);
	}
	closedir(dir);

	//stat test
	struct stat st;
	stat("initrd:/bin/hello",&st);
	printf("size : %ld\n",st.st_size);

	//try launching hello
	char **arg = {
		"initrd:/bin/hello",
		NULL
	};
	execve("initrd:/bin/hello",arg,NULL);

	//try open keayboard
	int kbd_fd = open("dev:/kb0",O_RDONLY);
	char shift = 0;
	char caps = 0;
	exit(0);

	for(;;){
		char c;
		size_t rsize = read(kbd_fd,&c,1);
		if(rsize < 1){
			continue;
		}

		switch (c)
		{
		case KEY_SHIFT:
			shift = 1;
			break;
		case KEY_SHIFT + 0x80:
			shift = 0;
			break;
		case KEY_CAPSLOCK:
			caps = 1 - caps;
			break;
		default:
			//is it a relase ?
			if(c & 0x80){
				continue;
			}

			if(shift || caps){
				putchar(toupper(c));
				break;
			}
			putchar(c);
			break;
		}
	}
	
	
	//assert(5 == 3);

	return 0;
}

