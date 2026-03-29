#include <twm-internal.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <twm.h>

int send_event(client_t *client, twm_event_t *event) {
	return send(client->fd, event, event->size, 0);
}

static void handle_init(client_t *client, twm_request_init_t *request) {
	if (request->major != TWM_CURRENT_MAJOR || request->minor != TWM_CURRENT_MINOR) {
		kick_client(client);
	}
}

static void handle_create_window(client_t *client, twm_request_create_window_t *request) {
	const char *title = strnlen(request->title, sizeof(request->title)) < sizeof(request->title) ? request->title : "window";
	
	window_t *window = create_window(client, NULL, request->width, request->height, title);

	twm_event_window_created_t event = {
		.base = {
			.request_id = request->base.id,
			.size       = sizeof(event),
		},
		.id = window->id,
	};
	send_event(client, (twm_event_t*)&event);
	return;
}

static void handle_destroy_window(client_t *client, twm_request_destroy_window_t *request) {
	window_t *window = get_window(request->id);
	if (!window) return;
	if (window->client != client->id) return;
	destroy_window(window);
}

static void handle_get_window_fb(client_t *client, twm_request_get_window_fb_t *request) {
	window_t *window = get_window(request->id);

	// TODO : maybee send error
	if (!window) return;
	if (window->client != client->id) return;

	twm_event_window_fb_t event = {
		.base = {
			.request_id = request->base.id,
			.size       = sizeof(event),
		},
		.fb_info = {
			.bpp = gfx->bpp,
			.red_mask_shift   = gfx->red_mask_shift,
			.red_mask_size    = gfx->red_mask_size,
			.green_mask_shift = gfx->green_mask_shift,
			.green_mask_size  = gfx->green_mask_size,
			.blue_mask_shift  = gfx->blue_mask_shift,
			.blue_mask_size   = gfx->blue_mask_size,
			.width = window->width,
			.height = window->height,
			.pitch = window->width * (gfx->bpp / 8),
		}
	};
	strcpy(event.path, window->framebuffer_path);
	send_event(client, (twm_event_t*)&event);
}

static void handle_redraw_window(client_t *client, twm_request_redraw_window_t *request) {
	window_t *window = get_window(request->id);
	if (!window) return;
	if (window->client != client->id) return;

	if (request->width  == TWM_WHOLE_WIDTH)  request->width = window->width;
	if (request->height == TWM_WHOLE_HEIGHT) request->height = window->height;

	if (request->x >= window->width) return;
	if (request->y >= window->height) return;

	//TODO : even more checking

	invalidate_rect(window->x + request->x, window->y + request->y, request->width, request->height);
}

static void handle_get_screen_fb(client_t *client, twm_request_get_screen_fb_t *request) {
	twm_event_screen_fb_t event = {
		.base = {
			.request_id = request->base.id,
			.size       = sizeof(event),
		},
		.fb_info = {
			.bpp = gfx->bpp,
			.red_mask_shift   = gfx->red_mask_shift,
			.red_mask_size    = gfx->red_mask_size,
			.green_mask_shift = gfx->green_mask_shift,
			.green_mask_size  = gfx->green_mask_size,
			.blue_mask_shift  = gfx->blue_mask_shift,
			.blue_mask_size   = gfx->blue_mask_size,
			.width = gfx->width,
			.height = gfx->height,
			.pitch = gfx->pitch,
		}
	};
	strcpy(event.path, getenv("FB"));
	send_event(client, (twm_event_t*)&event);
}


int handle_request(client_t *client){
	char buf[TWM_MAX_PACKET_SIZE];
	twm_request_t *request = (twm_request_t*)buf;
	ssize_t size = recv(client->fd, buf, sizeof(twm_request_t), 0);
	if (size < (ssize_t)sizeof(twm_request_t)) return -1;
	if (request->size > TWM_MAX_PACKET_SIZE) return -1;
	
	size = recv(client->fd, buf + sizeof(twm_request_t), request->size - sizeof(twm_request_t), 0);
	if (size < 0) return -1;

	switch (request->type) {
	case TWM_REQUEST_INIT:
		handle_init(client, (twm_request_init_t*)request);
		break;
	case TWM_REQUEST_CREATE_WINDOW:
		handle_create_window(client, (twm_request_create_window_t*)request);
		break;
	case TWM_REQUEST_DESTROY_WINDOW:
		handle_destroy_window(client, (twm_request_destroy_window_t*)request);
		break;
	case TWM_REQUEST_GET_WINDOW_FB:
		handle_get_window_fb(client, (twm_request_get_window_fb_t*)request);
		break;
	case TWM_REQUEST_REDRAW_WINDOW:
		handle_redraw_window(client, (twm_request_redraw_window_t*)request);
		break;
	case TWM_REQUEST_GET_SCREEN_FB:
		handle_get_screen_fb(client, (twm_request_get_screen_fb_t*)request);
		break;
	}
	
	return 0;
}
