#include <fcntl.h>
#include <syscall.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
//#include <assert.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/module.h>
#include <syscall.h>
#include <ctype.h>
#include <input.h>
#include <dirent.h>
#include <errno.h>
//stanix specific input device header
#include <input.h>

extern char **environ;

int main(int argc,char **argv){
	//init std streams
	open("/dev/null",O_RDONLY); //stdin
	open("/dev/tty0",O_WRONLY); //stdout
	open("/dev/tty0",O_WRONLY); //stderr


	printf("hello world !!\n");
	for (int i = 0; i < argc; i++){
		printf("arg %d : %s\n",i,argv[i]);
	}
	
	int fd = open("tmp:/test.txt",O_CREAT | O_RDWR | O_CLOEXEC);
	(void)fd;

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

	for(size_t i = 0; i < strlen("hello from pipe !!!\n"); i++){
		putchar(getc(pipe_read));
	}
	fclose(pipe_read);
	fclose(pipe_write);

	//try forking
	pid_t child = fork();
	if(!child){
		printf("i'am the child\n");
		exit(0);
	}
	if(child > 0){
		waitpid(child,NULL,0);
	}
	printf("i'am the parent\n");

	//stat test
	struct stat st;
	stat("bin/hello",&st);
	printf("size : %ld\n",st.st_size);

	//setup fake env
	putenv("PATH=/;/bin");
	putenv("USER=root");

	//env test
	printf("all env : \n");
	int i = 0;
	while(environ[i]){
		printf("%s\n",environ[i]);
		i++;
	}
	printf("kernel name : %s\n",getenv("KERNEL"));

	//test cwd
	char cwd[256];
	if(getcwd(cwd,256)){
		printf("cwd : %s\n",cwd);
	} else {
		perror("getcwd");
	}
	//try to cd into bin
	if(chdir("bin") < 0){
		perror("chdir(\"bin\")");
	}

	//try do some ls
	DIR *dir = opendir(".");
	if(!dir){
		//fprintf(stderr,"%s\n",strerror(errno));
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

	//try insmod
	const char *kargs[] = {
		"hello world !",
		"second arg",
		NULL
	};

	//launch tsh in the startup script
	const char *arg[] = {
		"/bin/tsh",
		"/startup.sh",
		NULL
	};

	execvp("/bin/tsh",arg);

	//try open keayboard
	int kbd_fd = open("/dev/kb0",O_RDONLY);
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
		if(event.ie_key.flags & IE_KEY_GRAPH){
			putchar(event.ie_key.c);
			continue;
		}

		
	}
	
	
	//assert(5 == 3);

	return 0;
}

