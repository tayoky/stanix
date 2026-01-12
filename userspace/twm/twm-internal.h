#ifndef _TWM_INTERNAL_H
#define _TWM_INTERNAL_H

#include <libutils/hashmap.h>
#include <gfx.h>
#include <twm.h>

typedef struct client {
	int fd;
} client_t;

typedef struct window {
	long width;
	long height;
	long x;
	long y;
	twm_window_t id;
	char *title;
	client_t *client;
	int framebuffer;
	uintptr_t fb_addr;
} window_t;

typedef struct theme {
	color_t primary;
	color_t secondary;
	color_t font_color;
	texture_t *cursor_texture;
	long titlebar_height;
	long border_width;
} theme_t;

typedef struct cursor {
	char *saved;
	long x;
	long y;
} cursor_t;

extern gfx_t *gfx;
extern theme_t theme;
extern font_t *font;
extern utils_hashmap_t windows;
extern cursor_t cursor;
extern int server_socket;
extern int kb;
extern int mouse;

void render_window_decor(window_t *window);
void render_window_content(window_t *window);
void init_cursor(cursor_t *cursor);
void render_and_move_cursor(cursor_t *cursor, long new_x, long new_y);
void error(const char *fmt, ...);
int handle_request(client_t *client);
void kick_client(client_t *client);
void handle_mouse(void);
void handle_keyboard(void);

#endif
