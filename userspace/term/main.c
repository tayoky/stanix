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
#include <sys/wait.h>
#include <libterm.h>
#include <tgui/tgui.h>

// GUI terminal emulator

#define KEY(key, str) [key - TGUI_KEY_FIRST] = str

char *keys2str[TGUI_KEY_LAST - TGUI_KEY_FIRST + 1] = {
	KEY(TGUI_KEY_HOME       , "\e[H"),
	KEY(TGUI_KEY_ARROW_UP   , "\e[A"),
	KEY(TGUI_KEY_ARROW_DOWN , "\e[B"),
	KEY(TGUI_KEY_ARROW_RIGHT, "\e[C"),
	KEY(TGUI_KEY_ARROW_LEFT , "\e[D"),
	KEY(TGUI_KEY_END        , "\e[F"),
	KEY(TGUI_KEY_INSERT, "\e[2~"),
	KEY(TGUI_KEY_DELETE, "\e[3~"),
};

term_t term;
font_t *font;
FILE *master_file;
int c_width;
int c_height;
tgui_window_t *window;
tgui_canva_t *canva;
int running = 1;
int crtl = 0;

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

gfx_t *get_gfx(void) {
	return tgui_canva_get_ctx(canva);
}

color_t term_color2gfx(term_color_t *term_color, int bg) {
	switch (term_color->type) {
	case TERM_COLOR_DEFAULT:
		return bg ? gfx_color(get_gfx(), 0, 0, 0) : gfx_color(get_gfx(), 0xff, 0xff, 0xff);
	case TERM_COLOR_ANSI:
		if (term_color->index < 16) {
			uint32_t col = ansi_colours[term_color->index];
			return gfx_color(get_gfx(), (col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
		} else if (term_color->index < 232) {
			uint8_t r = (term_color->index - 16) / 36 % 6 * 40 + 55;
			uint8_t g = (term_color->index - 16) / 6 % 6 * 40 + 55;
			uint8_t b = (term_color->index - 16) / 1 % 6 * 40 + 55;
			return gfx_color(get_gfx(), r, g, b);
		} else {
			//grey scale
			uint32_t color = (term_color->index - 232) * 10 + 8;
			return gfx_color(get_gfx(), color, color, color);
		}
	case TERM_COLOR_RGB:
		return gfx_color(get_gfx(), term_color->r, term_color->g, term_color->b);
	default:
		return gfx_color(get_gfx(), 0x80, 0x80, 0x80);
	}
}

void push_rect(long x, long y, long width, long height) {
	tgui_canva_set_dirty(canva, x, y, width, height);
}

void push_buffer(void) {
	tgui_canva_set_dirty(canva, 0, 0, canva->widget.width, canva->widget.height);
}

void draw_line(term_t *term, cell_t *cell, int y, int start_x, int end_x) {
	(void)term;
	for (int x=start_x; x < end_x; x++, cell++) {
		color_t bg_color = term_color2gfx(&cell->bg_color, 1);
		color_t fg_color = term_color2gfx(&cell->fg_color, 0);
		if (cell->attr & TERM_ATTR_INVERSE) {
			color_t tmp = bg_color;
			bg_color = fg_color;
			fg_color = tmp;
		}
		gfx_draw_rect(get_gfx(), bg_color, x * c_width, y * c_height, c_width, c_height);
		gfx_draw_char(get_gfx(), font, fg_color, x * c_width, y * c_height, cell->c);
	}
	push_rect(start_x * c_width, y * c_height, (end_x - start_x) * c_width, c_height);
}


void draw_cursor(term_t *term, int x, int y) {
	cell_t *cell = CELL_AT(term, x, y);
	gfx_draw_rect(get_gfx(), term_color2gfx(&cell->fg_color, 0), x * c_width, y * c_height, c_width, c_height);
	gfx_draw_char(get_gfx(), font, term_color2gfx(&cell->bg_color, 1), x * c_width, y * c_height, cell->c);
	push_rect(x * c_width, y * c_height, c_width, c_height);
}

void clear(term_t *term, term_rect_t *rect) {
	if (rect->x == 0 && rect->y == 0 && rect->width == term->width && rect->height == term->height) {
		gfx_clear(get_gfx(), term_color2gfx(&term->cursor.bg_color, 1));
		push_buffer();
	} else {
		gfx_draw_rect(get_gfx(), term_color2gfx(&term->cursor.bg_color, 1), rect->x * c_width, rect->y * c_height,
			rect->width * c_width, rect->height * c_height);
		push_rect(rect->x * c_width, rect->y * c_height, rect->width * c_width, rect->height * c_height);
	}
}

void move(term_t *term, term_rect_t *dest, term_rect_t *src) {
	if (dest->width == term->width) {
		memmove((void *)gfx_pixel_addr(get_gfx(), dest->x * c_width, dest->y * c_height), (void *)gfx_pixel_addr(get_gfx(), src->x * c_width, src->y * c_height),
			dest->width * dest->height * c_width * c_height * get_gfx()->bpp / 8);
		push_rect(dest->x * c_width, dest->y * c_height, dest->width * c_width, dest->height * c_height);
	} else {
		// TODO
	}
}

term_ops_t term_ops = {
	.draw_line   = draw_line,
	.draw_cursor = draw_cursor,
	.clear = clear,
	.move = move,
};

void key_press_callback(tobject_t *tobject, tgui_event_press_t *event) {
	(void)tobject;
	long key = event->sym;
	char buf[MB_CUR_MAX + 1];

	if (key == TGUI_KEY_LCRTL || key == INPUT_KEY_RCRTL) {
		crtl = 1;
		return ;
	}
	if (key >= TGUI_KEY_FIRST) {
		if (keys2str[key - TGUI_KEY_FIRST]) {
			fputs(keys2str[key - TGUI_KEY_FIRST], master_file);
			return;
		}
		return;
	}

	wctomb(buf, key);
	if (crtl && strlen(buf) == 1) {
		char c = tolower(buf[0]) - 'a' + 1;
		fputc(c, master_file);
	} else if (buf[0]) {
		fputs(buf, master_file);
	}

	return;
}

void key_release_callback(tobject_t *tobject, tgui_event_release_t *event) {
	(void)tobject;
	long key = event->sym;
	if (key == TGUI_KEY_LCRTL || key == INPUT_KEY_RCRTL) {
		crtl = 0;
		return;
	}
	return;
}


void close_callback(void) {
	running = 0;
}


int main(int argc, const char **argv) {
	(void)argc;
	(void)argv;
	printf("starting userspace terminal emulator...\n");

	if (!getenv("FONT")) {
		fprintf(stderr, "no FONT variable\n");
		return EXIT_FAILURE;
	}

	if (tgui_init() < 0) {
		fprintf(stderr, "could not initalize tgui\n");
		return EXIT_FAILURE;
	}
	window = tgui_window_new("terminal", 644, 480);
	canva = tgui_canva_new();
	tgui_widget_set_hexpand(TGUI_WIDGET_CAST(canva), TGUI_TRUE);
	tgui_widget_set_vexpand(TGUI_WIDGET_CAST(canva), TGUI_TRUE);
	tgui_window_set_child(window, TGUI_WIDGET_CAST(canva));
	tgui_widget_connect_signal(TGUI_WIDGET_CAST(window), "press", TCALLBACK_CAST(key_press_callback), NULL);
	tgui_widget_connect_signal(TGUI_WIDGET_CAST(window), "release", TCALLBACK_CAST(key_release_callback), NULL);
	tgui_widget_connect_signal(TGUI_WIDGET_CAST(window), "destroy", TCALLBACK_CAST(close_callback), NULL);
	tgui_render();

	font = gfx_load_font(NULL);
	if (!font) {
		perror(getenv("FONT"));
		return EXIT_FAILURE;
	}
	c_width  = gfx_char_width(font, ' ');
	c_height = gfx_char_height(font, ' ');

	// create a new pty
	struct winsize size = {
		.ws_xpixel = get_gfx()->width,
		.ws_ypixel = get_gfx()->height,
		.ws_col = get_gfx()->width / c_width,
		.ws_row = get_gfx()->height / c_height,
	};

	int master;
	int slave;
	if (openpty(&master, &slave, NULL, NULL, &size)) {
		perror("openpty");
		return EXIT_FAILURE;
	}

	// disable NL to NL CR converstion and other termios stuff
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
	term.width  = get_gfx()->width / c_width;
	term.height = get_gfx()->height / c_height;
	term.ops = &term_ops;
	term_init(&term);

	while (running) {
		tgui_render();
		struct pollfd wait[] = {
			{.fd = master,.events = POLLIN | POLLHUP,.revents = 0},
			{.fd = tgui_get_fd(),.events = POLLIN,.revents = 0}
		};

		if (poll(wait, 2, -1) < 0) {
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
		} else if (wait[0].revents & POLLHUP) {
			break;
		}

		if (wait[1].revents & POLLIN) {
			tgui_poll();
		}
	}

	tgui_widget_destroy(TGUI_WIDGET_CAST(window));
	tgui_fini();
	close(master);

	waitpid(child, NULL, WNOHANG);
	return 0;
}