#ifndef _TWM_INTERNAL_H
#define _TWM_INTERNAL_H

#include <libutils/hashmap.h>
#include <libinput.h>
#include <gfx.h>
#include <twm.h>

typedef struct client {
	int fd;
	int id;
} client_t;

typedef struct window {
	struct window *next;
	struct window *prev;
	long width;
	long height;
	long x;
	long y;
	long attribute;
	twm_window_t id;
	char *title;
	int client;
	char *framebuffer_path;
	void *framebuffer;
	struct window *parent;
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
	long x;
	long y;
} cursor_t;

extern gfx_t *gfx;
extern theme_t theme;
extern font_t *font;
extern utils_hashmap_t windows;
extern window_t *window_stack_top;
extern window_t *window_stack_bottom;
extern window_t *focus_window;
extern cursor_t cursor;
extern int server_socket;
extern libinput_keyboard_t *kb;
extern utils_vector_t clients;
extern int mouse;

void move_cursor(cursor_t *cursor, long new_x, long new_y);
void invalidate_rect(long x, long y, long width, long height);
void render(void);
void error(const char *fmt, ...);
int handle_request(client_t *client);
int accept_client(void);
void kick_client(client_t *client);
client_t *get_client(int id);
int send_event(client_t *client, twm_event_t *event);
void handle_mouse(void);
void handle_keyboard(void);
void push_window_at_top(window_t *window);
window_t *create_window(client_t *client, window_t *parent, long width, long height, const char *title);
void move_window(window_t *window, long new_x, long new_y);
void destroy_window(window_t *window);
window_t *get_window(twm_window_t id);
window_t *get_window_at(long x, long y);
int update_focus(window_t *window);
void set_grab(window_t *window, long offset_x, long offset_y);
int is_inside_window(window_t *window, long x, long y, long width, long height);

#endif
