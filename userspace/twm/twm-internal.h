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
	long titlebar_height;
	long border_width;
} theme_t;

extern gfx_t *gfx;
extern theme_t theme;
extern font_t *font;
extern utils_hashmap_t windows;
extern int server_socket;

void render_window_decor(window_t *window);
void render_window_content(window_t *window);
void error(const char *fmt, ...);
int handle_request(client_t *client);

#endif
