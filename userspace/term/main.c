#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <input.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pty.h>
#include <poll.h>
#include <termios.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/fb.h>
#include <sys/ioctl.h>

const char kbd_us[128] = {
	0,27,
	'1','2','3','4','5','6','7','8','9','0',
	'-','=',127,
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

const char kbd_us_shift[128] = {
	0, 27,
	'!','@','#','$','%','^','&','*','(',')',
	'_','+',127,
	'\t', /* tab */
	'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
	0, /* control */
	'A','S','D','F','G','H','J','K','L',':','"', '~',
	0, /* left shift */
	'|','Z','X','C','V','B','N','M','<','>','?',
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
	0, /* up */
	0, /* page up */
	'-',
	0, /* left arrow */
	0,
	0, /* right arrow */
	'+',
	0, /* 79 end */
	0, /* down */
	0, /* page down */
	0, /* insert */
	0, /* delete */
	0, 0, 0,
	0, /* F11 */
	0, /* F12 */
	0, /* everything else */
};

const char kbd_fr[128] = {
	0,27,
	'&','?' /*é*/,'"','\'','(','-',' ' /*è*/,'_',' '/*ç*/,' ' /*à*/,
	')','=',127,
	'\t', /* tab */
	'a','z','e','r','t','y','u','i','o','p','^','$','\n',
	0, /* control */
	'q','s','d','f','g','h','j','k','l','m','\'', '`',
	0, /* left shift */
	'<','w','x','c','v','b','n',',',';','/','!',
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

const char kbd_fr_shift[128] = {
	0, 27,
	'1','2','3','4','5','6','7','8','9','0',
	' ' /*°*/,'+',127,
	'\t', /* tab */
	'A','Z','E','R','T','Y','U','I','O','P','{','}','\n',
	0, /* control */
	'Q','S','D','F','G','H','J','K','L','M','%', '~',
	0, /* left shift */
	'>','W','X','C','V','B','N','?','.','/',' ' /*§*/,
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
	0, /* up */
	0, /* page up */
	'-',
	0, /* left arrow */
	0,
	0, /* right arrow */
	'+',
	0, /* 79 end */
	0, /* down */
	0, /* page down */
	0, /* insert */
	0, /* delete */
	0, 0, 0,
	0, /* F11 */
	0, /* F12 */
	0, /* everything else */
};

typedef struct {
    uint16_t magic; // Magic bytes for identification.
    uint8_t font_mode; // PSF font mode.
    uint8_t character_size; // PSF character size.
} PSF1_Header;

int x = 1;
int y = 1;
struct fb fb_info;
int fb;
char *font_data;
PSF1_Header font_header;
uint32_t front_color = 0xFFFFFF;
uint32_t back_color = 0x000000;
int ansi_escape_count = 0;
int ansi_escape_args[8];

uint32_t ansi_colours[] = {
	0xFFFFFF, //black
	0xFF0000, //red
	0x00FF00, //green
	0xFFFF00, //yellow
	0x0000FF, //blue
	0xFF00FF, //magenta
	0x00FFFF, //cyan
	0xFFFFFF, //white
	0x808080, //light black
	0x800000, //light red
	0x008000, //light green
	0x808000, //light yellow
	0x000080, //light blue
	0x800080, //light magenta
	0x008080, //light cyan
	0x808080, //light white
};

uint32_t parse_color(int id){
	if(ansi_escape_count >= 3 && ansi_escape_args[0] == id + 8){
		if(ansi_escape_args[2] < 16){
			return ansi_colours[ansi_escape_args[2]];
		} else if(ansi_escape_args[2] < 232) {
			uint8_t r = (ansi_escape_args[2] - 16) / 36 % 6 * 40 + 55;
			uint8_t g = (ansi_escape_args[2] - 16) /  6 % 6 * 40 + 55;
			uint8_t b = (ansi_escape_args[2] - 16) /  1 % 6 * 40 + 55;
			return r << 16 | g << 8 | b;
		} else if (ansi_escape_args[2] <= 255){
			//grey scale
			uint32_t color = (ansi_escape_args[2] - 232) * 10 + 8;
			color |= color << 16 | color << 8;
			return color;
		}
	} for(int i = 0; i<ansi_escape_count;i++){
		if(ansi_escape_args[i] >= id && ansi_escape_args[i] <= id + 7){
			return ansi_colours[ansi_escape_args[i] - id];
		}
	}
	return id == 30 ? 0xFFFFFF : 0x000000;
}

//most basic terminal emumator
void draw_char(char c){
	if(c == '\e'){
		memset(ansi_escape_args,0,sizeof(ansi_escape_args));
		ansi_escape_count = 1;
		return ;
	}
	if(ansi_escape_count == 1){
		if(c == '['){
			ansi_escape_count++;
			return;
		} else {
			ansi_escape_count = 0;
		}
	}
	
	if(ansi_escape_count){
		if(isdigit((unsigned char)c)){
			ansi_escape_args[ansi_escape_count-2] *= 10;
			ansi_escape_args[ansi_escape_count-2] += c - '0';
			return;
		} else if(c == ';'){
			ansi_escape_count++;
			return;
		} else {
			ansi_escape_count-=1;
			switch(c){
			case 'H':
				x = 1;
				y = 1;
				break;
			case 'J':
				lseek(fb,0,SEEK_SET);
				for (long i = 0; i < fb_info.width * fb_info.height; i++){
					write(fb,&back_color,sizeof(back_color));
				}
				break;
			case 'f':
				x = ansi_escape_args[1];
				y = ansi_escape_args[0];
				break;
			case 'm':
				if(ansi_escape_count == 1 && ansi_escape_args[0] == 0){
					back_color = 0x000000;
					front_color = 0xFFFFFF;
				} else if (ansi_escape_args[0] >= 40 && ansi_escape_args[0]<= 49){
					back_color = parse_color(40);
				} else if (ansi_escape_args[0] >= 30 && ansi_escape_args[0]<= 39){
					front_color = parse_color(30);
				}
				break;
			}
			ansi_escape_count = 0;
			return;
		}
	}
	
	switch(c){
	case '\n':
		x = 1;
		y++;
		return;
	case '\b':
		x--;
		return;
	}

	char *current_byte = &font_data[c * font_header.character_size];
	for (uint16_t i = 0; i < font_header.character_size; i++){
		uint32_t line[8];
		for (uint8_t j = 0; j < 8; j++){
			if(((*current_byte) >> (7 - j)) & 0x01){
				line[j] = front_color;
			} else {
				line[j] = back_color;
			}
		}
		lseek(fb,(((y - 1) * font_header.character_size + i) * fb_info.width + ((x - 1) * 8)) * sizeof(uint32_t),SEEK_SET);
		write(fb,line,sizeof(line));
		current_byte++;
	}

	x++;
}


int main(int argc,const char **argv){
	const char *layout = kbd_us;
	const char *layout_shift = kbd_us_shift;
	for (int i = 1; i < argc-1; i++){
		if((!strcmp(argv[i],"--layout")) || !strcmp(argv[i],"-i")){
			i++;
			if((!strcasecmp(argv[i],"azerty")) || !strcasecmp(argv[i],"french")){
				layout = kbd_fr;
				layout_shift = kbd_fr_shift;
			} else if((!strcasecmp(argv[i],"qwerty")) || !strcasecmp(argv[i],"english")){
				layout = kbd_us;
				layout = kbd_us_shift;
			}
		}
	}
	

	printf("starting userspace terminal emulator\n");

	//create a new pty
	int master;
	int slave;
	if(openpty(&master,&slave,NULL,NULL,NULL)){
		perror("openpty");
		return EXIT_FAILURE;
	}

	//disable NL to NL CR converstion and other termios stuff
	struct termios attr;
	if(tcgetattr(slave,&attr)){
		perror("tcgetattr");
	}
	attr.c_oflag &= ~ONLCR;
	//basicly we handle <CR> <CR NL> and <NL CR> and convert back to just <NL>
	attr.c_oflag |= OPOST | ONOCR | ONLRET | OCRNL;
	if(tcsetattr(slave,TCSANOW,&attr)){
		perror("tcsetattr");
	}

	//try open keyboard
	int kbd_fd = open("/dev/kb0",O_RDONLY);
	if(kbd_fd < 0){
		perror("/dev/kb0");
		return EXIT_FAILURE;
	}
	
	//try open framebuffer
	fb = open("/dev/fb0",O_WRONLY);
	if(fb < 0){
		perror("/dev/fb0");
		return EXIT_FAILURE;
	}
	if(ioctl(fb,IOCTL_GET_FB_INFO,&fb_info) < 0){
		perror("ioctl");
		return EXIT_FAILURE;
	}
	printf("framebuffer %ldx%ld \n",fb_info.width,fb_info.height);

	//load font
	int font_file = open("zap-light16.psf",O_RDONLY);
	if(font_file < 0){
		perror("/zap-light16.psf");
		return EXIT_FAILURE;
	}
	off_t font_size = lseek(font_file,0,SEEK_END);
	if(font_size < 0){
		perror("lseek");
	}
	font_data = malloc(font_size - sizeof(PSF1_Header));
	lseek(font_file,0,SEEK_SET);
	if(read(font_file,&font_header,sizeof(font_header)) < 0){
		perror("read");
	}
	if(read(font_file,font_data,font_size - sizeof(PSF1_Header)) < 0){
		perror("read");
	}
	close(font_file);
	printf("load font of size %ld\n",font_size);

	//fork and launch login with std stream set to the slave
	pid_t child = fork();
	if(!child){
		dup2(slave,STDIN_FILENO);
		dup2(slave,STDOUT_FILENO);
		dup2(slave,STDERR_FILENO);
		close(master);
		close(slave);
		close(kbd_fd);
		close(fb);

		const char *arg[] = {
			"/bin/login",
			NULL
		};
		execvp(arg[0],arg);
		perror("/bin/login");

		exit(EXIT_FAILURE);
	}

	close(slave);

	int crtl = 0;
	int shift = 0;

	for(;;){
		struct pollfd wait[] = {
			{.fd = master,.events = POLLIN,.revents = 0},
			{.fd = kbd_fd,.events = POLLIN,.revents = 0}
		};

		if(poll(wait,2,-1) < 0){
			perror("poll");
			return EXIT_FAILURE;
		}

		if(wait[0].revents & POLLIN){
			//there data to print
			char c;
			if(read(master,&c,1) < 0){
				//read error ???
				perror("read");
			} else {
				draw_char(c);
			}
		}

		if(wait[1].revents & POLLIN){
			//there keyboard data to read
			struct input_event event;

			if(read(kbd_fd,&event,sizeof(event)) < 1){
				goto ignore;
			}

			//ignore not key event
			if(event.ie_type != IE_KEY_EVENT){
				goto ignore;
			}

			//special case for crtl and shift
			if(event.ie_key.scancode == 0x1D){
				if(event.ie_key.flags & IE_KEY_RELEASE){
					crtl = 0;
				} else {
					crtl = 1;
				}
				goto ignore;
			}
			if(event.ie_key.scancode == 0x2A || event.ie_key.scancode == 0x36 || (event.ie_key.scancode == 0x3A && event.ie_key.flags & IE_KEY_PRESS)){
				shift = 1 - shift;
				goto ignore;
			}

			//ignore key release
			if(event.ie_key.flags & IE_KEY_RELEASE){
				goto ignore;
			}

			//put into the pipe and to the screen
			char c;
			if(shift){
				c = layout_shift[event.ie_key.scancode];
			} else {
				c = layout[event.ie_key.scancode];
			}
			if(c){
				//if crtl is pressed send special crtl + XXX char
				if(crtl){
					c -= 'a' - 1;
				}
				if(write(master,&c,1)){
					//if broken pipe that mean the child probably exited
					//nobody need us now
					if(errno == EPIPE){
						return EXIT_SUCCESS;
					}
				}
				ignore:
			}
		}
	}
}