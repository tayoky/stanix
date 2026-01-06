#ifndef _TWM_H
#define _TWM_H

#include <stdint.h>

typedef struct twm_request {
	uint64_t id;
	int size;
	int type;
} twm_request_t;

#define TWM_REQUEST_INIT           1
#define TWM_REQUEST_CREATE_WINDOW  2
#define TWM_REQUEST_DESTROY_WINDOW 3
#define TWM_REQUEST_SET_WINDOW     4

#define TWM_WINDOW_SHOW   1
#define TWM_WINDOW_WIDTH  2
#define TWM_WINDOW_HEIGHT 3
#define TWM_WINDOW_X      4
#define TWM_WINDOW_Y      5

typedef struct twm_event {
	uint64_t request_id;
	int size;
} twm_event_t;

typedef struct twm_ctx {
	uint64_t id_count;
	int fd;
} twm_ctx_t;

typedef int16_t twm_window_t;

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
} twm_request_create_window_t;


typedef struct twm_event_window_created {
	twm_event_t base;
	twm_window_t id;
} twm_event_window_created_t;

#define TWM_CURRENT_MAJOR 0
#define TWM_CURRENT_MINOR 1
#define TWM_MAX_PACKET_SIZE 4096

int twm_init(const char *path);
void twm_fini(void);
int twm_send_request(twm_request_t *request);
twm_window_t twm_create_window(const char *title, long width, long height);
int twm_destroy_window(twm_window_t window);
int twm_set_window_property(twm_window_t window, int property, long value);
twm_event_t *twm_poll_event(void);
void twm_handle_event(twm_event_t *event);

#endif