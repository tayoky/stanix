#ifndef TWM_INTERNAL_H
#define TWM_INTERNAL_H

#include <libutils/hashmap.h>
#include <libutils/list.h>
#include <libinput.h>
#include <gfx.h>
#include <twm.h>

typedef struct client {
	int fd;
	int id;
} client_t;

typedef struct screen {
	utils_list_node_t node;
	twm_screen_t id;
	gfx_t *gfx;
	long invalidate_start_x;
	long invalidate_start_y;
	long invalidate_end_x;
	long invalidate_end_y;
	long x;
	long y;
} screen_t;

typedef struct window {
	utils_list_node_t node;
	struct window *parent;
	long width;
	long height;
	long x;
	long y;
	long attribute;
	twm_window_t id;
	char *title;
	int client;
	int zindex;
	int framebuffer_is_old; // the framebuffer is old and new to be updated
	int framebuffer_fd;
	twm_fb_info_t fb_info;
	char *framebuffer_path;
	void *framebuffer;
} window_t;

#define WINDOW_FROM_NODE(node) ((window_t*)(node))

typedef struct theme {
	color_t primary;
	color_t secondary;
	color_t font_color;
	texture_t *cursor_texture;
	long padding;
	long border_width;
	long button_width;
	long titlebar_height;
} theme_t;

typedef struct cursor {
	long x;
	long y;
} cursor_t;

extern theme_t theme;
extern font_t *font;
extern utils_hashmap_t windows;
extern utils_list_t window_stacks[TWM_ZINDEX_COUNT];
extern size_t screens_count;
extern utils_hashmap_t screens;
extern window_t *focus_window;
extern int grab_input;
extern cursor_t cursor;
extern int server_socket;
extern libinput_keyboard_t *kb;
extern utils_vector_t clients;
extern int mouse;
extern int desktop_hook;

void move_cursor(cursor_t *cursor, long new_x, long new_y);
void invalidate_rect(long x, long y, long width, long height);
void render(void);
void error(const char *fmt, ...);
int handle_request(client_t *client);
int accept_client(void);
void kick_client(client_t *client);
client_t *get_client(int id);
int send_event(client_t *client, twm_event_t *event);
int send_event_id(int id, twm_event_t *event);
void handle_mouse(void);
void handle_keyboard(void);
void push_window_at_top(window_t *window);
window_t *create_window(client_t *client, window_t *parent, long width, long height, const char *title);
void move_window(window_t *window, long new_x, long new_y);
void window_set_title(window_t *window, const char *title);
void window_set_zindex(window_t *window, int zindex);
void window_get_inner_bounds(window_t *window, long *x, long *y, long *width, long *height);
void window_get_bounds(window_t *window, long *x, long *y, long *width, long *height);
void destroy_window(window_t *window);
void window_get_fb(window_t *window, twm_fb_info_t *info, const char **framebuffer_path);
void window_mark_framebuffer_old(window_t *window);
void window_set_size(window_t *window, long width, long height);
window_t *get_window(twm_window_t id);
window_t *get_window_at(long x, long y);
void set_window_attr(window_t *window, long attr);
int update_focus(window_t *window);
void set_grab(window_t *window, long offset_x, long offset_y);
int is_inside_window(window_t *window, long x, long y, long width, long height);
void screen_add(screen_t *screen);
void screen_remove(screen_t *screen);
void screen_init(void);
void send_screens(void);
screen_t *get_screen(twm_screen_t id);
void send_screens(client_t *client);
screen_t *get_screen_at(long x, long y);

#endif
