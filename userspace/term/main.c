#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <input.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pty.h>
#include <gfx.h>
#include <poll.h>
#include <termios.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

//most basic terminal emumator

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

struct cell {
	int c;
	color_t back_color;
	color_t front_color;
};

gfx_t *fb;
int x = 1;
int y = 1;
int width;
int height;
struct cell *grid;
color_t front_color;
color_t back_color;
font_t *font;
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

#define ansi2gfx(col) gfx_color(fb,(col >> 16) & 0xff,(col >> 8) & 0xff,col & 0xff)

color_t parse_color(int id){
	if(ansi_escape_count >= 3 && ansi_escape_args[0] == id + 8){
		if(ansi_escape_args[2] < 16){
			return ansi2gfx(ansi_colours[ansi_escape_args[2]]);
		} else if(ansi_escape_args[2] < 232) {
			uint8_t r = (ansi_escape_args[2] - 16) / 36 % 6 * 40 + 55;
			uint8_t g = (ansi_escape_args[2] - 16) /  6 % 6 * 40 + 55;
			uint8_t b = (ansi_escape_args[2] - 16) /  1 % 6 * 40 + 55;
			return gfx_color(fb,r,g,b);
		} else if (ansi_escape_args[2] <= 255){
			//grey scale
			uint32_t color = (ansi_escape_args[2] - 232) * 10 + 8;
			return gfx_color(fb,color,color,color);
		}
	} for(int i = 0; i<ansi_escape_count;i++){
		if(ansi_escape_args[i] >= id && ansi_escape_args[i] <= id + 7){
			return ansi2gfx(ansi_colours[ansi_escape_args[i] - id]);
		}
	}
	return id == 30 ? gfx_color(fb,255,255,255) : gfx_color(fb,0,0,0);
}

void redraw(int cx,int cy){
	struct cell cell = grid[(cy - 1) * width +  cx - 1];
	gfx_draw_char(fb,font,cell.back_color,cell.front_color,(cx-1) * gfx_char_width(font,' '),(cy-1) * gfx_char_height(font,' '),cell.c);
}

void redraw_cursor(int cx,int cy){
	struct cell cell = grid[(cy - 1) * width +  cx - 1];
	gfx_draw_char(fb,font,cell.front_color,cell.back_color,(cx-1) * gfx_char_width(font,' '),(cy-1) * gfx_char_height(font,' '),cell.c);
}

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
				for(int i =0;i < width * height; i++){
					grid[i].c = ' ';
					grid[i].c = back_color;
				}
				gfx_clear(fb,back_color);
				break;
			case 'f':
				x = ansi_escape_args[1];
				y = ansi_escape_args[0];
				break;
			case 'm':
				if(ansi_escape_count == 1 && ansi_escape_args[0] == 0){
					back_color = 0x000000;
					front_color = 0xFFFFFF;
				} else if ((ansi_escape_args[0] >= 40 && ansi_escape_args[0]<= 49) || (ansi_escape_count >= 2 && ansi_escape_args[1] >= 40 && ansi_escape_args[1]<= 49)){
					back_color = parse_color(40);
				} else if ((ansi_escape_args[0] >= 30 && ansi_escape_args[0]<= 39) || (ansi_escape_count >= 2 && ansi_escape_args[1] >= 30 && ansi_escape_args[1]<= 39)){
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
		redraw(x,y);
		x = 1;
		y++;
		redraw_cursor(x,y);
		return;
	case '\b':
		redraw(x,y);
		x--;
		redraw_cursor(x,y);
		return;
	case '\t':
		redraw(x,y);
		x += 8 - (x % 8);
		redraw_cursor(x,y);
		return;
	}

	//put into grid
	grid[(y - 1) * width +  x - 1].c = (unsigned char) c;
	grid[(y - 1) * width +  x - 1].back_color = back_color;
	grid[(y - 1) * width +  x - 1].front_color = front_color;
	redraw(x,y);
	x++;
	redraw_cursor(x,y);
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
	

	printf("starting userspace terminal emulator...\n");

	//open gtx context
	fb = gfx_open_framebuffer("/dev/fb0");
	if(!fb){
		perror("open gfx context");
		return EXIT_FAILURE;
	}

	//load font
	font = gfx_load_font("/zap-light16.psf");
	if(!font){
		perror("/zap-light16.psf");
		return EXIT_FAILURE;
	}
	

	//create a new pty
	struct winsize size = {
		.ws_xpixel = fb->width,
		.ws_ypixel = fb->height,
		.ws_col = fb->width / gfx_char_width(font,' '),
		.ws_row = fb->height / gfx_char_height(font,' '),
	};

	int master;
	int slave;
	if(openpty(&master,&slave,NULL,NULL,&size)){
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


	//fork and launch login with std stream set to the slave
	pid_t child = fork();
	if(!child){
		dup2(slave,STDIN_FILENO);
		dup2(slave,STDOUT_FILENO);
		dup2(slave,STDERR_FILENO);
		close(master);
		close(slave);
		close(kbd_fd);
		gfx_free(fb);

		const char *arg[] = {
			"/bin/login",
			NULL
		};
		execvp(arg[0],arg);
		perror("/bin/login");

		exit(EXIT_FAILURE);
	}

	close(slave);

	//clear screen and init color
	front_color = gfx_color(fb,255,255,255);
	back_color  = gfx_color(fb,0,0,0);
	gfx_clear(fb,back_color);
	gfx_push_buffer(fb);
	gfx_disable_backbuffer(fb);

	//create an empty grid
	width = fb->width / gfx_char_width(font,' ');
	height = fb->height / gfx_char_height(font,' ');
	grid = malloc(sizeof(struct cell) * width * height);
	for(int i = 0;i < width * height; i++){
			grid[i].c = ' ';
			grid[i].back_color = back_color;
			grid[i].front_color = front_color;
	}

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