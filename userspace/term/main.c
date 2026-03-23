#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <input.h>
#include <libinput.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pty.h>
#include <gfx.h>
#include <twm.h>
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
		0,"*","w","x","c","v","b","n",",",";",":","!",0,"*",
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
		0,"µ","W","X","C","V","B","N","?",".","/","§",0,"*",
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

#define KEY(key, str) [key - INPUT_KEY_FIRST] = str

char *keys2str[INPUT_KEY_LAST - INPUT_KEY_FIRST + 1] = {
	KEY(INPUT_KEY_ESC, "\e"),
	KEY(INPUT_KEY_ENTER, "\n"),
	KEY(INPUT_KEY_TAB, "\t"),
	KEY(INPUT_KEY_BACKSPACE, "\177"),
	KEY(INPUT_KEY_HOME       , "\e[H"),
	KEY(INPUT_KEY_ARROW_UP   , "\e[A"),
	KEY(INPUT_KEY_ARROW_DOWN , "\e[B"),
	KEY(INPUT_KEY_ARROW_RIGHT, "\e[C"),
	KEY(INPUT_KEY_ARROW_LEFT , "\e[D"),
	KEY(INPUT_KEY_END        , "\e[F"),
	KEY(INPUT_KEY_DELETE, "\e[3~"),
};

term_t term;
gfx_t *gfx;
font_t *font;
FILE *master_file;
int c_width;
int c_height;
int use_twm;
libinput_keyboard_t *keyboard;
twm_window_t window;

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
		return bg ? gfx_color(gfx, 0, 0, 0) : gfx_color(gfx, 0xff, 0xff, 0xff);
	case TERM_COLOR_ANSI:
		if (term_color->index < 16) {
			uint32_t col = ansi_colours[term_color->index];
			return gfx_color(gfx, (col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
		} else if (term_color->index < 232) {
			uint8_t r = (term_color->index - 16) / 36 % 6 * 40 + 55;
			uint8_t g = (term_color->index - 16) / 6 % 6 * 40 + 55;
			uint8_t b = (term_color->index - 16) / 1 % 6 * 40 + 55;
			return gfx_color(gfx, r, g, b);
		} else {
			//grey scale
			uint32_t color = (term_color->index - 232) * 10 + 8;
			return gfx_color(gfx, color, color, color);
		}
	case TERM_COLOR_RGB:
		return gfx_color(gfx, term_color->r, term_color->g, term_color->b);
	default:
		return gfx_color(gfx, 0x80, 0x80, 0x80);
	}
}

void push_rect(long x, long y, long width, long height) {
	if (use_twm) {
		twm_redraw_window(window, x, y, width, height);
	} else {
		gfx_push_rect(gfx, x, y, width, height);
	}
}

void push_buffer(void) {
	if (use_twm) {
		twm_redraw_window(window, 0, 0, TWM_WHOLE_WIDTH, TWM_WHOLE_HEIGHT);
	} else {
		gfx_push_buffer(gfx);
	}
}

void draw_cell(term_t *term, cell_t *cell, int x, int y) {
	(void)term;
	color_t bg_color = term_color2gfx(&cell->bg_color, 1);
	color_t fg_color = term_color2gfx(&cell->fg_color, 0);
	if (cell->attr & TERM_ATTR_INVERSE) {
		color_t tmp = bg_color;
		bg_color = fg_color;
		fg_color = tmp;
	}
	gfx_draw_rect(gfx, bg_color, x * c_width, y * c_height, c_width, c_height);
	gfx_draw_char(gfx, font, fg_color, x * c_width, y * c_height, cell->c);
	push_rect(x * c_width, y * c_height, c_width, c_height);
}

void draw_cursor(term_t *term, int x, int y) {
	cell_t *cell = CELL_AT(term, x, y);
	gfx_draw_rect(gfx, term_color2gfx(&cell->fg_color, 0), x * c_width, y * c_height, c_width, c_height);
	gfx_draw_char(gfx, font, term_color2gfx(&cell->bg_color, 1), x * c_width, y * c_height, cell->c);
	push_rect(x * c_width, y * c_height, c_width, c_height);
}

void clear(term_t *term, term_rect_t *rect) {
	if (rect->x == 0 && rect->y == 0 && rect->width == term->width && rect->height == term->height) {
		gfx_clear(gfx, term_color2gfx(&term->cursor.bg_color, 1));
		push_buffer();
	} else {
		gfx_draw_rect(gfx, term_color2gfx(&term->cursor.bg_color, 1), rect->x * c_width, rect->y * c_height,
			rect->width * c_width, rect->height * c_height);
		push_rect(rect->x * c_width, rect->y * c_height, rect->width * c_width, rect->height * c_height);
	}
}

void move(term_t *term, term_rect_t *dest, term_rect_t *src) {
	if (dest->width == term->width) {
		memmove((void *)gfx_pixel_addr(gfx, dest->x * c_width, dest->y * c_height), (void *)gfx_pixel_addr(gfx, src->x * c_width, src->y * c_height),
			dest->width * dest->height * c_width * c_height * gfx->bpp / 8);
		push_rect(dest->x * c_width, dest->y * c_height, dest->width * c_width, dest->height * c_height);
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
int main(int argc, const char **argv) {
	(void)argc;
	(void)argv;
	printf("starting userspace terminal emulator...\n");

	if (!getenv("FONT")) {
		fprintf(stderr, "no FONT variable\n");
		return EXIT_FAILURE;
	}

	// twm support
	if (getenv("TWM")) {
		use_twm = 1;
		if (twm_init(NULL) < 0) {
			fprintf(stderr, "fail to connect to twm server\n");
			return EXIT_FAILURE;
		}
		window = twm_create_window("term", 640, 480);
		gfx = twm_get_window_gfx(window);
	} else if (getenv("FB")) {
		use_twm = 0;
		gfx = gfx_open_framebuffer(NULL);

		// try to open keyboard
		keyboard = libinput_open_keyboard("/dev/kb0", O_RDONLY | O_CLOEXEC);
		if (!keyboard) {
			perror("/dev/kb0");
			return EXIT_FAILURE;
		}
	} else {
		fprintf(stderr, "no FB or TWM variable\n");
		return EXIT_FAILURE;
	}
	if (!gfx) {
		perror("open gfx context");
		return EXIT_FAILURE;
	}

	//load font
	font = gfx_load_font(NULL);
	if (!font) {
		perror(getenv("FONT"));
		return EXIT_FAILURE;
	}
	c_width  = gfx_char_width(font, ' ');
	c_height = gfx_char_height(font, ' ');


	//create a new pty
	struct winsize size = {
		.ws_xpixel = gfx->width,
		.ws_ypixel = gfx->height,
		.ws_col = gfx->width / c_width,
		.ws_row = gfx->height / c_height,
	};

	int master;
	int slave;
	if (openpty(&master, &slave, NULL, NULL, &size)) {
		perror("openpty");
		return EXIT_FAILURE;
	}

	//disable NL to NL CR converstion and other termios stuff
	struct termios attr;
	if (tcgetattr(slave, &attr)) {
		perror("tcgetattr");
	}
	attr.c_oflag &= ~(ONLCR | OCRNL | ONOCR);
	attr.c_oflag |= OPOST;
	if (tcsetattr(slave, TCSANOW, &attr)) {
		perror("tcsetattr");
	}


	//fork and launch login with std stream set to the slave
	pid_t child = fork();
	if (!child) {
		dup2(slave, STDIN_FILENO);
		dup2(slave, STDOUT_FILENO);
		dup2(slave, STDERR_FILENO);
		close(master);
		close(slave);
		gfx_free(gfx);

		//skip login with -f
		static char *arg[] = {
			"/bin/login",
			"-f",
			NULL
		};
		execvp(arg[0], arg);
		perror("/bin/login");

		exit(EXIT_FAILURE);
	}

	close(slave);
	master_file = fdopen(master, "r+");
	setvbuf(master_file, NULL, _IONBF, 0);

	// init term
	term.width  = gfx->width / c_width;
	term.height = gfx->height / c_height;
	term.ops = &term_ops;
	term_init(&term);

	int crtl = 0;

	for (;;) {
		struct pollfd wait[] = {
			{.fd = master,.events = POLLIN,.revents = 0},
			{.fd = keyboard->fd,.events = POLLIN,.revents = 0}
		};

		if (poll(wait, use_twm ? 1 : 2, -1) < 0) {
			perror("poll");
			return EXIT_FAILURE;
		}

		if (wait[0].revents & POLLIN) {
			//there data to print
			char buf[1024];
			ssize_t s = read(master, buf, sizeof(buf));
			if (s > 0) {
				term_output(&term, buf, s);
			}
		}

		if (!use_twm && (wait[1].revents & POLLIN)) {
			// there is keyboard data to read
			struct input_event event;
			if (libinput_get_keyboard_event(keyboard, &event) < 0) goto ignore;

			// ignore not key event
			if (event.ie_type != IE_KEY_EVENT) {
				goto ignore;
			}

			if (event.ie_key.key == INPUT_KEY_LCRTL || event.ie_key.key == INPUT_KEY_RCRTL) {
				if (event.ie_key.flags & IE_KEY_RELEASE) {
					crtl = 0;
				} else {
					crtl = 1;
				}
				goto ignore;
			}

			// ignore key release
			if (event.ie_key.flags & IE_KEY_RELEASE) {
				goto ignore;
			}

			// put into the pty
			char *str = NULL;
			char buf[MB_CUR_MAX + 1];
			if (event.ie_key.key >= INPUT_KEY_FIRST) {
				str = keys2str[event.ie_key.key - INPUT_KEY_FIRST];
			} else if (libinput_is_graph_key(event.ie_key.key)) {
				int len = wctomb(buf, event.ie_key.key);
				buf[len] = '\0';
				str = buf;
			}
			if (str) {
				//if crtl is pressed send special crtl + XXX char
				if (crtl && strlen(str) == 1) {
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