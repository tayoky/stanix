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
#include <tlibc.h>
//stanix specific input device header
#include <input.h>

int main(int argc,char **argv){
	__init_tlibc(argc,argv,0,NULL);
	
	//init std streams
	open("dev:/null",O_RDONLY); //stdin
	open("dev:/console",O_WRONLY); //stdout
	open("dev:/console",O_WRONLY); //stderr


	printf("hello world !!\n");
	for (size_t i = 0; i < argc; i++){
		printf("arg %d : %s\n",i,argv[i]);
	}
	
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

	//try launching doom
	char *arg[] = {
		"initrd:/bin/hello",
		"-iwad",
		"doom1.wad",
		NULL
	};

	//execvp("initrd:/bin/hello",arg);

	//try open keayboard
	int kbd_fd = open("dev:/kb0",O_RDONLY);
	char shift = 0;
	char caps = 0;
	#define ESC '\033'

	for(;;){
		struct input_event event;

		if(read(kbd_fd,&event,sizeof(event)) < 1){
			continue;
		}

		//ignore not key event
		if(event.ie_type != IE_KEY_EVENT){
			continue;
		}

		//ignore key release
		if(event.ie_key.flags & IE_KEY_RELEASE){
			continue;
		}

		//print if possible
		if(event.ie_key.flags & IE_KEY_GRPAH){
			putchar(event.ie_key.c);
			continue;
		}

		
	}
	
	
	//assert(5 == 3);

	return 0;
}

