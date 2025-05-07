#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <input.h>
#include <fcntl.h>
#include <errno.h>

//most basic terminal emumator
int main(int argc,const char **argv){
	printf("starting userspace terminal emulator\n");

	int pipefd[2];
	pipe(pipefd);

	dup2(pipefd[0],STDIN_FILENO);
	close(pipefd[0]);

	//try open keyboard
	int kbd_fd = open("/dev/kb0",O_RDONLY);
	if(kbd_fd < 0){
		perror("/dev/kb0");
		exit(1);
	}

	pid_t child = fork();
	if(!child){
		close(pipefd[1]);

		//if arg launch the specified program
		if(argc > 1){
			execvp(argv[1],&argv[1]);
			perror(argv[1]);
		} else {
			//by default launch login
			const char *arg[] = {
				"/bin/login",
				NULL
			};
			execvp(arg[0],arg);
			perror("/bin/login");
		}
		exit(EXIT_FAILURE);
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
			if(write(pipefd[1],&event.ie_key.c,1)){
				//if broken pipe that mean the child probably exited
				//nobody need us now
				if(errno == EPIPE){
					return EXIT_SUCCESS;
				}
			}
			putchar(event.ie_key.c);
			fflush(stdout);
			continue;
		}
	}
}