#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <input.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

const char kbd_us[128] = {
	0,0,
	'1','2','3','4','5','6','7','8','9','0',
	'-','=','\b',
	'\t', /* tab */
	'q','w','e','r','t','y','u','i','o','p','[',']','\n',
	0, /* control */
	'a','s','d','f','g','h','j','k','l',';','\'', '`',
	0, /* left shift */
	'\\','z','x','c','v','b','n','m',',','.','/',
	0, /* right shift */
	'*',
	0, /* alt */
	' ', /* space */
	0, /* caps lock */
	0, /* F1 [59] */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, /* ... F10 */
	0, /* 69 num lock */
	0, /* scroll lock */
	0, /* home */
	0, /* up arrow */
	0, /* page up */
	'-',
	0, /* left arrow */
	0,
	0, /* right arrow */
	'+',
	0, /* 79 end */
	0, /* down arrow */
	0, /* page down */
	0, /* insert */
	0, /* delete */
	0, 0, 0,
	0, /* F11 */
	0, /* F12 */
	0, /* everything else */
};

const char kbd_fr[128] = {
	0,0,
	'1','2','3','4','5','6','7','8','9','0',
	'-','=','\b',
	'\t', /* tab */
	'a','e','e','r','t','y','u','i','o','p','^','$','\n',
	0, /* control */
	'q','s','d','f','g','h','j','k','l','m','\'', '`',
	0, /* left shift */
	'\\','w','x','c','v','b','n',',',';','/','!',
	0, /* right shift */
	'*',
	0, /* alt */
	' ', /* space */
	0, /* caps lock */
	0, /* F1 [59] */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, /* ... F10 */
	0, /* 69 num lock */
	0, /* scroll lock */
	0, /* home */
	0, /* up arrow */
	0, /* page up */
	'-',
	0, /* left arrow */
	0,
	0, /* right arrow */
	'+',
	0, /* 79 end */
	0, /* down arrow */
	0, /* page down */
	0, /* insert */
	0, /* delete */
	0, 0, 0,
	0, /* F11 */
	0, /* F12 */
	0, /* everything else */
};

//most basic terminal emumator
int main(int argc,const char **argv){
	const char *layout = kbd_us;
	for (size_t i = 1; i < argc-1; i++){
		if((!strcmp(argv[i],"--layout")) || !strcmp(argv[i],"-i")){
			i++;
			if((!stricmp(argv[i],"azerty")) || !stricmp(argv[i],"french")){
				layout = kbd_fr;
			} else if((!stricmp(argv[i],"qwerty")) || !stricmp(argv[i],"english")){
				layout = kbd_us;
			}
		}
	}
	

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

		//launch login
		const char *arg[] = {
			"/bin/login",
			NULL
		};
		execvp(arg[0],arg);
		perror("/bin/login");

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
		char c = layout[event.ie_key.scancode];
		if(c){
			if(write(pipefd[1],&c,1)){
				//if broken pipe that mean the child probably exited
				//nobody need us now
				if(errno == EPIPE){
					return EXIT_SUCCESS;
				}
			}
			putchar(c);
			fflush(stdout);
			continue;
		}
	}
}