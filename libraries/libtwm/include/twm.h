#ifndef _TWM_H
#define _TWM_H

#include <stdint.h>
#include <limits.h>

struct gfx_context;

typedef struct twm_fb_info {
	long width;
	long height;
	long pitch;
	int bpp;
	int red_mask_shift;
	int red_mask_size;
	int green_mask_shift;
	int green_mask_size;
	int blue_mask_shift;
	int blue_mask_size;
} twm_fb_info_t;

typedef struct twm_request {
	uint64_t id;
	int size;
	int type;
} twm_request_t;

#define TWM_REQUEST_INIT            1
#define TWM_REQUEST_CREATE_WINDOW   2
#define TWM_REQUEST_DESTROY_WINDOW  3
#define TWM_REQUEST_GET_WINDOW_FB   4
#define TWM_REQUEST_GET_WINDOW_ATTR 5
#define TWM_REQUEST_SET_WINDOW_ATTR 6
#define TWM_REQUEST_REDRAW_WINDOW   7
#define TWM_REQUEST_COUNT           8

#define TWM_WINDOW_SHOW   1
#define TWM_WINDOW_WIDTH  2
#define TWM_WINDOW_HEIGHT 3
#define TWM_WINDOW_X      4
#define TWM_WINDOW_Y      5

typedef struct twm_event {
	uint64_t request_id;
	int size;
	int type;
} twm_event_t;

#define TWM_EVENT_INIT           0
#define TWM_EVENT_WINDOW_CREATED 1
#define TWM_EVENT_WINDOW_FB      2
#define TWM_EVENT_WINDOW_ATTR    3
#define TWM_EVENT_WINDOW_RESIZED 4
#define TWM_EVENT_WINDOW_CLOSED  5
#define TWM_EVENT_WINDOW_FOCUS   6
#define TWM_EVENT_INPUT          7
#define TWM_EVENT_COUNT          8

typedef struct twm_ctx {
	uint64_t id_count;
	int fd;
} twm_ctx_t;

typedef int16_t twm_window_t;
typedef int16_t twm_device_t;

// requests

typedef struct twm_request_init {
	twm_request_t base;
	int major;
	int minor;
} twm_request_init_t;

typedef struct twm_request_create_window {
	twm_request_t base;
	char title[32];
	long width;
	long height;
	twm_window_t parent;
} twm_request_create_window_t;


typedef struct twm_request_destroy_window {
	twm_request_t base;
	twm_window_t id;
} twm_request_destroy_window_t;

typedef struct twm_request_get_window_fb {
	twm_request_t base;
	twm_window_t id;
} twm_request_get_window_fb_t;

typedef struct twm_request_set_window_attr {
	twm_request_t base;
	twm_window_t id;
	long attr;
	int how;
} twm_request_set_window_attr_t;

#define TWM_SET_ATTR    0
#define TWM_ADD_ATTR    1
#define TWM_REMOVE_ATTR 2
#define TWM_ATTR_RESIZABLE 0
#define TWM_ATTR_SHOW      2
#define TWM_ATTR_BORDER    3

typedef struct twm_request_get_window_attr {
	twm_request_t base;
	twm_window_t id;
} twm_request_get_window_attr_t;

typedef struct twm_request_redraw_window {
	twm_request_t base;
	twm_window_t id;
	long x;
	long y;
	long width;
	long height;
} twm_request_redraw_window_t;

// events/reponses

typedef struct twm_event_window_created {
	twm_event_t base;
	twm_window_t id;
} twm_event_window_created_t;

typedef struct twm_event_window_fb {
	twm_event_t base;
	twm_window_t id;
	twm_fb_info_t fb_info;
	char path[256];
} twm_event_window_fb_t;

typedef struct twm_event_window_attr {
	twm_event_t base;
	twm_window_t id;
	long attr;
} twm_event_window_attr_t;

typedef struct twm_event_input {
	twm_event_t base;
	twm_device_t device;
	twm_window_t window;
	int type;
	union {
		struct {
			unsigned long key;
			unsigned long scancode;
			int flags;
		} key;
		struct {
			unsigned long axis;
			int rex_x;
			int rel_y;
			int abs_x;
			int abs_y;
		} move;
	};
} twm_event_input_t;

#define TWM_INPUT_KEY  0
#define TWM_INPUT_MOVE 1
#define TWM_INPUT_PRESS   0x01
#define TWM_INPUT_RELEASE 0x02
#define TWM_INPUT_HOLD    0x04

#define TWM_CURRENT_MAJOR 0
#define TWM_CURRENT_MINOR 1
#define TWM_MAX_PACKET_SIZE 4096
#define TWM_WHOLE_WIDTH INT_MAX
#define TWM_WHOLE_HEIGHT INT_MAX

typedef void (*twm_handler_t)(twm_event_t *event, void *arg);
#define TWM_HANDLER(handler) ((twm_handler_t)(void*)(handler))

int twm_init(const char *path);
void twm_fini(void);
int twm_get_fd(void);
int twm_send_request(twm_request_t *request);
twm_window_t twm_create_window(const char *title, long width, long height);
int twm_destroy_window(twm_window_t window);
int twm_get_window_fb(twm_window_t window, int *fd, twm_fb_info_t *fb_info);
int twm_set_window_attr(twm_window_t window, int how, long attr);
long twm_get_window_attr(twm_window_t window);
int twm_redraw_window(twm_window_t window, long x, long y, long width, long height);
struct gfx_context *twm_get_window_gfx(twm_window_t window);
twm_event_t *twm_poll_event(void);
void twm_handle_event(twm_event_t *event);
void twm_set_handler(int event_type, twm_handler_t hadnler, void *data);

#endif