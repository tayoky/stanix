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
	'<','w','x','c','v','b','n',',',';',':','!',
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
	[0x56] = '<',
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
	[0x56] = '>',
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

int flags;
#define FLAG_CURSOR 0x01
#define FLAG_INVERT 0x02

gfx_t *fb;
int x = 1;
int y = 1;
int width;
int height;
int c_width;
int c_height;
struct cell *grid;
color_t front_color;
color_t back_color;
font_t *font;
int ansi_escape_count = 0;
int ansi_escape_args[8];

uint32_t ansi_colours[] = {
	0x000000, //black
	0xD00000, //red
	0x00D000, //green
	0xD0D000, //yellow
	0x0000D0, //blue
	0xD000D0, //magenta
	0x00D0D0, //cyan
	0xD0D0D0, //white
	0x808080, //light black
	0xFF0000, //light red
	0x00FF00, //light green
	0xFFFF00, //light yellow
	0x0000FF, //light blue
	0xFF00FF, //light magenta
	0x00FFFF, //light cyan
	0xFFFFFF, //light white
};

#define ansi2gfx(col) gfx_color(fb,(col >> 16) & 0xff,(col >> 8) & 0xff,col & 0xff)

color_t parse_complex(int i){
	if(ansi_escape_args[i + 1] < 16){
		return ansi2gfx(ansi_colours[ansi_escape_args[i + 1]]);
	} else if(ansi_escape_args[i + 1] < 232) {
		uint8_t r = (ansi_escape_args[i + 1] - 16) / 36 % 6 * 40 + 55;
		uint8_t g = (ansi_escape_args[i + 1] - 16) /  6 % 6 * 40 + 55;
		uint8_t b = (ansi_escape_args[i + 1] - 16) /  1 % 6 * 40 + 55;
		return gfx_color(fb,r,g,b);
	} else if (ansi_escape_args[i + 1] <= 255){
		//grey scale
		uint32_t color = (ansi_escape_args[i + 1] - 232) * 10 + 8;
		return gfx_color(fb,color,color,color);
	}

	return 0x808080;
}

void parse_color(void){
	for(int i = 0; i<ansi_escape_count;i++){
		switch(ansi_escape_args[i]){
		case 0:
			back_color = ansi2gfx(ansi_colours[0]);
			front_color = ansi2gfx(ansi_colours[7]);
			continue;
		case 38:
			i++;
			if(ansi_escape_count - i >= 2)
			front_color = parse_complex(i);
			continue;
		case 39:
			front_color = ansi2gfx(ansi_colours[7]);
			continue;
		case 48:
			i++;
			if(ansi_escape_count - i >= 2)
			back_color = parse_complex(i);
			continue;
		case 49:
			back_color = ansi2gfx(ansi_colours[0]);
			continue;
		}
		if(ansi_escape_args[i] >= 30 && ansi_escape_args[i] <= 37){
			front_color = ansi2gfx(ansi_colours[ansi_escape_args[i] - 30]);
		}
		if(ansi_escape_args[i] >= 40 && ansi_escape_args[i] <= 47){
			back_color = ansi2gfx(ansi_colours[ansi_escape_args[i] - 40]);
		}
	}
}

void redraw(int cx,int cy){
	struct cell cell = grid[(cy - 1) * width +  cx - 1];
	gfx_draw_rect(fb,cell.back_color,(cx-1) * c_width,(cy-1) * c_height,c_width,c_height);
	gfx_draw_char(fb,font,cell.front_color,(cx-1) * c_width,(cy-1) * c_height,cell.c);
	gfx_push_rect(fb,(cx-1) * c_width,(cy-1) * c_height,c_width,c_height);
}

void redraw_cursor(int cx,int cy){
	if(!(flags & FLAG_CURSOR))return redraw(cx,cy);
	struct cell cell = grid[(cy - 1) * width +  cx - 1];
	gfx_draw_rect(fb,cell.front_color,(cx-1) * c_width,(cy-1) * c_height,c_width,c_height);
	gfx_draw_char(fb,font,cell.back_color,(cx-1) * c_width,(cy-1) * c_height,cell.c);
	gfx_push_rect(fb,(cx-1) * c_width,(cy-1) * c_height,c_width,c_height);
}

void scroll(int s){
	memmove(grid,&grid[s * width],(height - s) * width * sizeof(struct cell));
	for(int i=(height - s) * width;i < width * height; i++){
		grid[i].c = ' ';
		grid[i].back_color = back_color;
		grid[i].front_color = front_color;
	}

	memcpy(fb->backbuffer,(char *)fb->backbuffer + fb->pitch * s * c_height,fb->pitch * (fb->height - s * c_height));
	gfx_draw_rect(fb,back_color,0,fb->height - s * c_height,fb->width,s * c_height);
	gfx_push_buffer(fb);
	y -= s;
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
		} else if(c == '?'){
			return;
		} else if(c == ';'){
			ansi_escape_count++;
			return;
		} else {
			ansi_escape_count-=1;
			switch(c){
			case 'H':
				if(ansi_escape_count < 2){
					redraw(x,y);
					x = 1;
					y = 1;
					redraw_cursor(x,y);
					break;
				}
				//fallthrough
			case 'f':
				redraw(x,y);
				x = ansi_escape_args[1];
				y = ansi_escape_args[0];
				if(x > width) x = width;
				if(y > height) y = height;
				redraw_cursor(x,y);
				break;
			case 'G':
				redraw(x,y);
				y = ansi_escape_args[0];
				if(y > height)y = height;
				redraw_cursor(x,y);
				break;
			case 'F':
				redraw(x,y);
				x = 0;
				//fallthrough
			case 'A':
				redraw(x,y);
				y -= ansi_escape_args[0];
				if(y < 1)y = 1;
				redraw_cursor(x,y);
				break;
			case 'E':
				redraw(x,y);
				x = 0;
				//fallthrough
			case 'B':
				redraw(x,y);
				y += ansi_escape_args[0];
				if(y > height)y = height;
				redraw_cursor(x,y);
				break;
			case 'D':
				redraw(x,y);
				x -= ansi_escape_args[0];
				if(x < 1)x = 1;
				redraw_cursor(x,y);
				break;
			case 'C':
				redraw(x,y);
				x += ansi_escape_args[0];
				if(x > width)x = width;
				redraw_cursor(x,y);
				break;
			case 'J':
				for(int i =0;i < width * height; i++){
					grid[i].c = ' ';
					grid[i].back_color = back_color;
					grid[i].front_color = front_color;
				}
				gfx_clear(fb,back_color);
				gfx_push_buffer(fb);
				break;
			case 'm':
				parse_color();
				break;
			case 'l':
				if(ansi_escape_count < 1)break;
				switch(ansi_escape_args[0]){
				case 25:
					flags &= ~FLAG_CURSOR;
					break;
				}
				break;
			case 'h':
				if(ansi_escape_count < 1)break;
				switch(ansi_escape_args[0]){
				case 25:
					flags |= FLAG_CURSOR;
					break;
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
		if(y > height){
			scroll(y - height);
		}
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

void writestr(int fd,const char *str){
	write(fd,str,strlen(str));
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
	c_width  = gfx_char_width(font,' ');
	c_height = gfx_char_height(font,' ');
	

	//create a new pty
	struct winsize size = {
		.ws_xpixel = fb->width,
		.ws_ypixel = fb->height,
		.ws_col = fb->width / c_width,
		.ws_row = fb->height / c_height,
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
	attr.c_oflag &= ~(ONLCR | OCRNL | ONOCR);
	attr.c_oflag |= OPOST;
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

		//skip login with -f
		static const char *arg[] = {
			"/bin/login",
			"-f",
			NULL
		};
		execvp(arg[0],arg);
		perror("/bin/login");

		exit(EXIT_FAILURE);
	}

	close(slave);

	//clear screen and init color
	front_color = ansi2gfx(ansi_colours[7]);
	back_color  = ansi2gfx(ansi_colours[0]);
	gfx_clear(fb,back_color);
	gfx_push_buffer(fb);

	//create an empty grid and init flags
	flags = FLAG_CURSOR;
	width = fb->width / c_width;
	height = fb->height / c_height;
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
			if(event.ie_key.scancode >= 0x80){
				c = 0;
			} else if(shift){
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
			} else {
				switch(event.ie_key.scancode){
				//up arrow
				case 0x48 + 0x80:
					writestr(master,"\e[A");
					break;
				//down arrow
				case 0x50 + 0x80:
					writestr(master,"\e[B");
					break;
				//right arrow
				case 0x4D + 0x80:
					writestr(master,"\e[C");
					break;
				//left arrow
				case 0x4B + 0x80:
					writestr(master,"\e[D");
					break;
				}
			}
		}
	}
}