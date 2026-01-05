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
#include <wchar.h>
#include <sys/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <libterm.h>

//most basic terminal emumator

struct layout {
	char *keys[256];
	char *shift[256];
	char *altgr[256];
};


struct layout kbd_us = {
	.keys = {
		0,"\e","1","2","3","4","5","6","7","8","9","0","-","=","\177",
		"\t","q","w","e","r","t","y","u","i","o","p","[","]","\n",
		0,"a","s","d","f","g","h","j","k","l",";","\"", "`",
		0,"\\","z","x","c","v","b","n","m",",",".","/",0,"*",
		0," ",
		[0x47] =
		"7","8","9","-"
		"4","5","6","+"
		"1","2","3","0",
		[0x47 + 0x80] = "\e[H", //home	
		[0x48 + 0x80] = "\e[A", //up arrow
		[0x50 + 0x80] = "\e[B", //down arrow
		[0x4D + 0x80] = "\e[C", //right arrow
		[0x4B + 0x80] = "\e[D", //left arrow
		[0x4F + 0x80] = "\e[F", //end
	},
	.shift = {
		0,"\e","!","@","#","$","%","^","&","*","(",")","_","+","\177",
		"\t","Q","W","E","R","T","Y","U","I","O","P","{","}","\n",
		0,"A","S","D","F","G","H","J","K","L",":","\"", "~",
		0,"|","Z","X","C","V","B","N","M","<",">","?",0,"*",
		0," ",
		[0x47] =
		"7","8","9","-",
		"4","5","6","+",
		"1","2","3","0",
		[0x47 + 0x80] = "\e[H", //home	
		[0x48 + 0x80] = "\e[A", //up arrow
		[0x50 + 0x80] = "\e[B", //down arrow
		[0x4D + 0x80] = "\e[C", //right arrow
		[0x4B + 0x80] = "\e[D", //left arrow
		[0x4F + 0x80] = "\e[F", //end
	},
};
struct layout kbd_fr = {
		.keys = {
		0,"\e","&","é","\"","\'","(","-","è","_","ç","à",")","=","\177",
		"\t","a","z","e","r","t","y","u","i","o","p","^","$","\n",
		0,"q","s","d","f","g","h","j","k","l","m","\"", "`",
		0,"<","w","x","c","v","b","n",",",";",":","!",0,"*",
		0," ",
		[0x47] =
		"7","8","9","-",
		"4","5","6","+",
		"1","2","3","0",
		[0x47 + 0x80] = "\e[H", //home	
		[0x48 + 0x80] = "\e[A", //up arrow
		[0x50 + 0x80] = "\e[B", //down arrow
		[0x4D + 0x80] = "\e[C", //right arrow
		[0x4B + 0x80] = "\e[D", //left arrow
		[0x4F + 0x80] = "\e[F", //end
		[0x56] = "<",
	},
	.shift = {
		0,"\e","1","2","3","4","5","6","7","8","9","0","°","+","\177",
		"\t","A","Z","E","R","T","Y","U","I","O","P","{","}","\n",
		0,"Q","S","D","F","G","H","J","K","L","M","%", "~",
		0,">","W","X","C","V","B","N","?",".","/","§",0,"*",
		0," ",
		[0x47] =
		"7","8","9","-",
		"4","5","6","+",
		"1","2","3","0",
		[0x47 + 0x80] = "\e[H", //home	
		[0x48 + 0x80] = "\e[A", //up arrow
		[0x50 + 0x80] = "\e[B", //down arrow
		[0x4D + 0x80] = "\e[C", //right arrow
		[0x4B + 0x80] = "\e[D", //left arrow
		[0x4F + 0x80] = "\e[F", //end
		[0x56] = ">",
	},
	.altgr = {
		0,0,0,"~","#","{","[","|","`","\\","^","@","]","}",0,
	},
};

term_t term;
gfx_t *fb;
font_t *font;
FILE *master_file;
int c_width;
int c_height;

uint32_t ansi_colours[] = {
	0x000000, //black
	0xD00000, //red
	0x00D000, //green
	0xD0D000, //yellow
	0x0050D0, //blue  we put a bit of green otherwise it'ws not readable
	0xD000D0, //magenta
	0x00D0D0, //cyan
	0xD0D0D0, //white
	0x808080, //light black
	0xFF0000, //light red
	0x00FF00, //light green
	0xFFFF00, //light yellow
	0x0050FF, //light blue we put a bit of green for same reason as dark blue
	0xFF00FF, //light magenta
	0x00FFFF, //light cyan
	0xFFFFFF, //light white
};


color_t term_color2gfx(term_color_t *term_color, int bg) {
	switch (term_color->type) {
	case TERM_COLOR_DEFAULT:
		return bg ?  gfx_color(fb, 0, 0, 0) : gfx_color(fb, 0xff, 0xff, 0xff);
	case TERM_COLOR_ANSI:
		if (term_color->index < 16) {
			uint32_t col = ansi_colours[term_color->index];
			return gfx_color(fb, (col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
		} else if(term_color->index < 232) {
			uint8_t r = (term_color->index - 16) / 36 % 6 * 40 + 55;
			uint8_t g = (term_color->index - 16) /  6 % 6 * 40 + 55;
			uint8_t b = (term_color->index - 16) /  1 % 6 * 40 + 55;
			return gfx_color(fb, r, g, b);
		} else {
			//grey scale
			uint32_t color = (term_color->index - 232) * 10 + 8;
			return gfx_color(fb, color, color, color);
		}
	case TERM_COLOR_RGB:
		return gfx_color(fb, term_color->r, term_color->g, term_color->b);
	default:
		return gfx_color(fb, 0x80, 0x80, 0x80);
	}
}

void draw_cell(term_t *term, cell_t *cell, int x, int y) {
	(void)term;
	gfx_draw_rect(fb, term_color2gfx(&cell->bg_color, 1), x * c_width, y * c_height, c_width, c_height);
	gfx_draw_char(fb, font, term_color2gfx(&cell->fg_color, 0), x * c_width, y * c_height, cell->c);
	gfx_push_rect(fb, x * c_width, y * c_height, c_width, c_height);
}

void draw_cursor(term_t *term, int x, int y) {
	cell_t *cell = CELL_AT(term, x, y);
	gfx_draw_rect(fb, term_color2gfx(&cell->fg_color, 0), x * c_width, y * c_height, c_width, c_height);
	gfx_draw_char(fb, font, term_color2gfx(&cell->bg_color, 1), x * c_width, y * c_height, cell->c);
	gfx_push_rect(fb, x * c_width, y * c_height, c_width, c_height);
}

void clear(term_t *term, term_rect_t *rect) {
	if (rect->x == 0 && rect->y == 0 && rect->width == term->width && rect->height  == term->height) {
		gfx_clear(fb, term_color2gfx(&term->cursor.bg_color, 1));
		gfx_push_buffer(fb);
	} else {
		gfx_draw_rect(fb, term_color2gfx(&term->cursor.bg_color, 1), rect->x * c_width, rect->y * c_height, 
		rect->width * c_width, rect->height * c_height);
		gfx_push_rect(fb, rect->x * c_width, rect->y * c_height, rect->width * c_width, rect->height * c_height);
	}
}

void move(term_t *term, term_rect_t *dest, term_rect_t *src) {
	if (dest->width == term->width) {
		memmove((void*)gfx_pixel_addr(fb, dest->x * c_width, dest->y * c_height), (void*)gfx_pixel_addr(fb, src->x * c_width, src->y * c_height), 
		dest->width * dest->height * c_width * c_height * fb->bpp / 8);
		gfx_push_rect(fb, dest->x * c_width, dest->y * c_height, dest->width * c_width, dest->height * c_height);
	} else {
		// TODO
	}
}

term_ops_t term_ops = {
	.draw_cell   = draw_cell,
	.draw_cursor = draw_cursor,
	.clear = clear,
	.move = move,
};

//TODO : terminate when child die
int main(int argc,const char **argv){
	struct layout *layout = &kbd_us;
	for (int i = 1; i < argc-1; i++){
		if((!strcmp(argv[i],"--layout")) || !strcmp(argv[i],"-i")){
			i++;
			if((!strcasecmp(argv[i],"azerty")) || !strcasecmp(argv[i],"french")){
				layout = &kbd_fr;
			} else if((!strcasecmp(argv[i],"qwerty")) || !strcasecmp(argv[i],"english")){
				layout = &kbd_us;
			}
		}
	}

	printf("starting userspace terminal emulator...\n");

	if(!getenv("FB") || !getenv("FONT")){
		fprintf(stderr,"no FB or FONT variable\n");
		return EXIT_FAILURE;
	}

	//open gtx context
	fb = gfx_open_framebuffer(NULL);
	if(!fb){
		perror("open gfx context");
		return EXIT_FAILURE;
	}

	//load font
	font = gfx_load_font(NULL);
	if(!font){
		perror(getenv("FONT"));
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
		static char *arg[] = {
			"/bin/login",
			"-f",
			NULL
		};
		execvp(arg[0],arg);
		perror("/bin/login");

		exit(EXIT_FAILURE);
	}

	close(slave);
	master_file = fdopen(master, "r+");
	setvbuf(master_file, NULL, _IONBF, 0);

	// init term
	term.width  = fb->width / c_width;
	term.height = fb->height / c_height;
	term.ops = &term_ops;
	term_init(&term);

	int crtl = 0;
	int shift = 0;
	int altgr = 0;

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
			char buf[1024];
			ssize_t s = read(master, buf, sizeof(buf));
			if (s > 0) {
				term_output(&term, buf, s);
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
			//alt gr
			if(event.ie_key.scancode == 0x80 + 0x38){
				if(event.ie_key.flags & IE_KEY_RELEASE){
					altgr = 0;
				} else {
					altgr = 1;
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
			char *str;
			if(altgr && layout->altgr[event.ie_key.scancode]){
				str = layout->altgr[event.ie_key.scancode];
			} else {
				if(shift){
					str = layout->shift[event.ie_key.scancode];
				} else {
					str = layout->keys[event.ie_key.scancode];
				}
			}
			if(str){
				//if crtl is pressed send special crtl + XXX char
				if(crtl && strlen(str) == 1){
					char c = str[0] - 'a' + 1;
					fputc(c, master_file);
				} else {
					fputs(str, master_file);
				}
				ignore:
			}
		}
	}
}