#include <twm-internal.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <twm.h>

int send_event(client_t *client, twm_event_t *event) {
	if (!client) return 0;
	return send(client->fd, event, event->size, 0);
}

int send_event_id(int id, twm_event_t *event) {
	return send_event(get_client(id), event);
}

static void handle_init(client_t *client, twm_request_init_t *request) {
	if (request->major != TWM_CURRENT_MAJOR || request->minor != TWM_CURRENT_MINOR) {
		kick_client(client);
	}
	// send a list of currently connected screens
	send_screens(client);
}

static void handle_create_window(client_t *client, twm_request_create_window_t *request) {
	const char *title = strnlen(request->title, sizeof(request->title)) < sizeof(request->title) ? request->title : "window";

	window_t *parent = get_window(request->parent);
	window_t *window = create_window(client, parent, request->width, request->height, title);

	twm_event_window_created_t event = {
		.base = {
			.request_id = request->base.id,
			.size       = sizeof(event),
		},
		.id = window->id,
	};
	send_event(client, (twm_event_t *)&event);
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
	if (!window) return;
	if (window->client != client->id) return;

	twm_fb_info_t fb_info;
	const char *framebuffer_path;
	window_get_fb(window, &fb_info, &framebuffer_path);

	twm_event_window_fb_t event = {
		.base = {
			.request_id = request->base.id,
			.size       = sizeof(event),
			.type = TWM_EVENT_WINDOW_FB,
		},
		.fb_info = fb_info,
	};
	printf("got path %s\n", framebuffer_path);
	strcpy(event.path, framebuffer_path);
	send_event(client, (twm_event_t *)&event);
}

static void handle_get_window_attr(client_t *client, twm_request_get_window_attr_t *request) {
	window_t *window = get_window(request->id);
	if (!window) return;
	if (window->client != client->id && client->id != desktop_hook) return;

	twm_event_window_attr_t event = {
		.base = {
			.request_id = request->base.id,
			.size       = sizeof(event),
			.type = TWM_EVENT_WINDOW_ATTR,
		},
		.attr = {
			.attr   = window->attribute,
			.x      = window->x,
			.y      = window->y,
			.id     = window->id,
			.parent = window->parent ? window->parent->id : TWM_NULL,
			.zindex = window->zindex,
		},
	};
	strcpy(event.attr.title, window->title);
	send_event(client, (twm_event_t *)&event);
}

static void handle_set_window_attr(client_t *client, twm_request_set_window_attr_t *request) {
	window_t *window = get_window(request->id);
	if (!window) return;
	if (window->client != client->id && client->id != desktop_hook) return;

	long attr = window->attribute;
	switch (request->how) {
	case TWM_SET_ATTR:
		attr = request->attr;
		break;
	case TWM_ADD_ATTR:
		attr |= request->attr;
		break;
	case TWM_REMOVE_ATTR:
		attr &= ~request->attr;
		break;
	}
	set_window_attr(window, attr);
}

static void handle_set_window_pos(client_t *client, twm_request_set_window_pos_t *request) {
	window_t *window = get_window(request->id);
	if (!window) return;
	if (window->client != client->id) return;

	move_window(window, request->x, request->y);
}

static void handle_set_window_title(client_t *client, twm_request_set_window_title_t *request) {
	const char *title = strnlen(request->title, sizeof(request->title)) < sizeof(request->title) ? request->title : "window";
	window_t *window = get_window(request->id);
	if (!window) return;
	if (window->client != client->id) return;

	window_set_title(window, title);
}

static void handle_set_window_size(client_t *client, twm_request_set_window_size_t *request) {
	window_t *window = get_window(request->id);
	if (!window) return;
	if (window->client != client->id && desktop_hook != client->id) return;

	window_set_size(window, request->width, request->height);
}

static void handle_set_window_zindex(client_t *client, twm_request_set_window_zindex_t *request) {
	window_t *window = get_window(request->window);
	if (!window) return;
	if (window->client != client->id && desktop_hook != client->id) return;

	if (request->zindex < TWM_ZINDEX_MIN || request->zindex > TWM_ZINDEX_MAX) return;
	window_set_zindex(window, request->zindex);
}

static void handle_redraw_window(client_t *client, twm_request_redraw_window_t *request) {
	window_t *window = get_window(request->id);
	if (!window) return;
	if (window->client != client->id) return;

	long win_x, win_y, win_width, win_height;
	window_get_inner_bounds(window, &win_x, &win_y, &win_width, &win_height);

	if (request->width == TWM_WHOLE_WIDTH)   request->width  = win_width;
	if (request->height == TWM_WHOLE_HEIGHT) request->height = win_height;

	if (request->x >= win_width) return;
	if (request->y >= win_height) return;

	//TODO : even more checking
	invalidate_rect(win_x + request->x, win_y + request->y, request->width, request->height);
}

static void handle_get_screen_attr(client_t *client, twm_request_get_screen_attr_t *request) {
	screen_t *screen = get_screen(request->id);
	if (!screen) return;

	gfx_t *gfx = screen->gfx;
	twm_event_screen_attr_t event = {
		.base = {
			.request_id = request->base.id,
			.size       = sizeof(event),
		},
		.attr = {
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
			},
			.x = screen->x,
			.y = screen->y,
		}
	};
	strcpy(event.attr.name, screen->name);
	send_event(client, (twm_event_t *)&event);
}

static void handle_start_dragging(client_t *client, twm_request_start_dragging_t *request) {
	window_t *window = get_window(request->id);
	if (!window) return;
	if (window->client != client->id) return;

	set_grab(window, request->offset_x, request->offset_y);
}

static void handle_grab_desktop_hook(client_t *client, twm_request_grab_desktop_hook_t *request) {
	(void)request;
	if (desktop_hook) return;
	desktop_hook = client->id;
	printf("client grabbed desktop hook\n");
}

static void handle_grab_input(client_t *client, twm_request_grab_input_t *request) {
	if (request->window == TWM_NULL) {
		if (focus_window->client != client->id && desktop_hook != client->id) return;
		grab_input = 0;
		return;
	}
	window_t *window = get_window(request->window);
	if (!window) return;
	update_focus(window);
	grab_input = 1;
}


int handle_request(client_t *client) {
	char buf[TWM_MAX_PACKET_SIZE];
	twm_request_t *request = (twm_request_t *)buf;
	ssize_t size = recv(client->fd, buf, sizeof(twm_request_t), 0);
	if (size < (ssize_t)sizeof(twm_request_t)) return -1;
	if (request->size > TWM_MAX_PACKET_SIZE) return -1;

	size = recv(client->fd, buf + sizeof(twm_request_t), request->size - sizeof(twm_request_t), 0);
	if (size < 0) return -1;

	switch (request->type) {
	case TWM_REQUEST_INIT:
		handle_init(client, (twm_request_init_t *)request);
		break;
	case TWM_REQUEST_CREATE_WINDOW:
		handle_create_window(client, (twm_request_create_window_t *)request);
		break;
	case TWM_REQUEST_DESTROY_WINDOW:
		handle_destroy_window(client, (twm_request_destroy_window_t *)request);
		break;
	case TWM_REQUEST_GET_WINDOW_FB:
		handle_get_window_fb(client, (twm_request_get_window_fb_t *)request);
		break;
	case TWM_REQUEST_GET_WINDOW_ATTR:
		handle_get_window_attr(client, (twm_request_get_window_attr_t *)request);
		break;
	case TWM_REQUEST_SET_WINDOW_ATTR:
		handle_set_window_attr(client, (twm_request_set_window_attr_t *)request);
		break;
	case TWM_REQUEST_SET_WINDOW_POS:
		handle_set_window_pos(client, (twm_request_set_window_pos_t *)request);
		break;
	case TWM_REQUEST_SET_WINDOW_TITLE:
		handle_set_window_title(client, (twm_request_set_window_title_t *)request);
		break;
	case TWM_REQUEST_SET_WINDOW_SIZE:
		handle_set_window_size(client, (twm_request_set_window_size_t *)request);
		break;
	case TWM_REQUEST_SET_WINDOW_ZINDEX:
		handle_set_window_zindex(client, (twm_request_set_window_zindex_t *)request);
		break;
	case TWM_REQUEST_REDRAW_WINDOW:
		handle_redraw_window(client, (twm_request_redraw_window_t *)request);
		break;
	case TWM_REQUEST_GET_SCREEN_ATTR:
		handle_get_screen_attr(client, (twm_request_get_screen_fb_t *)attr);
		break;
	case TWM_REQUEST_START_DRAGGING:
		handle_start_dragging(client, (twm_request_start_dragging_t *)request);
		break;
	case TWM_REQUEST_GRAB_DESKTOP_HOOK:
		handle_grab_desktop_hook(client, (twm_request_grab_desktop_hook_t *)request);
		break;
	case TWM_REQUEST_GRAB_INPUT:
		handle_grab_input(client, (twm_request_grab_input_t *)request);
		break;
	}

	return 0;
}
