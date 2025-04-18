#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <input.h>
#include <fcntl.h>

//most basic terminal emumator
int main(int argc,char **argv){
	(void)argc;
	(void)argv;
	printf("starting userspace terminal emulator\n");
	int pipefd[2];
	pipe(pipefd);

	dup2(pipefd[0],STDIN_FILENO);

	//try open keyboard
	int kbd_fd = open("/dev/kb0",O_RDONLY);

	if(kbd_fd < 0){
		perror("/dev/kb0");
		exit(1);
	}

	pid_t child = fork();
	if(!child){
		//launch tsh because no login
		const char *arg[] = {
			"/bin/tsh",
			NULL
		};
		execv(arg[0],arg);
		perror("/bin/tsh");
	}

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

		//put into the pipe and to the screen
		if(event.ie_key.flags & IE_KEY_GRAPH){
			write(kbd_fd,&event.ie_key.c,1);
			putchar(event.ie_key.c);
			continue;
		}
		
	}

}