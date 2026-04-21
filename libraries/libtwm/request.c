#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <twm.h>

extern twm_ctx_t ctx;

static void *wait_for_response(uint64_t id) {
	twm_event_t *event;
	for (;;) {
		event = twm_poll_event();
		if (!event) return NULL;
		if (event->request_id != id) {
			twm_handle_event((twm_event_t*)event);
			free(event);
			continue;
		}
		break;
	}
	// we got the event !
	return event;
}

int twm_send_request(twm_request_t *request) {
	request->id = ctx.id_count++;
	return send(ctx.fd, request, request->size, 0);
}

twm_window_t twm_create_window(const char *title, long width, long height, twm_window_t parent) {
	twm_request_create_window_t request = {
		.base = {
			.type = TWM_REQUEST_CREATE_WINDOW,
			.size = sizeof(request),
		},
		.width  = width,
		.height = height,
		.parent = parent,
	};
	strncpy(request.title, title, sizeof(request.title) - 1);

	int ret = twm_send_request((twm_request_t*)&request);
	if (ret < 0) return ret;

	twm_event_window_created_t *event = wait_for_response(request.base.id);
	twm_window_t window = event->id;
	free(event);
	return window;
}


int twm_destroy_window(twm_window_t window) {
	twm_request_destroy_window_t request = {
		.base = {
			.type = TWM_REQUEST_DESTROY_WINDOW,
			.size = sizeof(request),
		},
		.id = window,
	};

	return twm_send_request((twm_request_t*)&request);
}

int twm_get_window_fb(twm_window_t window, int *fd, twm_fb_info_t *fb_info) {
	twm_request_get_window_fb_t request = {
		.base = {
			.type = TWM_REQUEST_GET_WINDOW_FB,
			.size = sizeof(request),
		},
		.id = window,
	};
	
	int ret = twm_send_request((twm_request_t*)&request);
	if (ret < 0) return ret;

	twm_event_window_fb_t *event = wait_for_response(request.base.id);

	*fb_info = event->fb_info;

	// we need to open the framebuffer
	*fd = shm_open(event->path, O_WRONLY, 0);
	if (*fd < 0) {
		free(event);
		return -1;
	}

	free(event);
	return 0;
}

int twm_set_window_attr(twm_window_t window, int how, long attr) {
	twm_request_set_window_attr_t request = {
		.base = {
			.type = TWM_REQUEST_SET_WINDOW_ATTR,
			.size = sizeof(request),
		},
		.id = window,
		.how = how,
		.attr = attr,
	};

	return twm_send_request((twm_request_t*)&request);
}

int twm_get_window_attr(twm_window_t window, twm_window_attr_t *attr) {
	twm_request_set_window_attr_t request = {
		.base = {
			.type = TWM_REQUEST_GET_WINDOW_ATTR,
			.size = sizeof(request),
		},
		.id = window,
	};
	
	int ret = twm_send_request((twm_request_t*)&request);
	if (ret < 0) return ret;

	twm_event_window_attr_t *event = wait_for_response(request.base.id);
	*attr = event->attr;
	free(event);
	return 0;
}

int twm_set_window_pos(twm_window_t window, long x, long y) {
	twm_request_set_window_pos_t request = {
		.base = {
			.type = TWM_REQUEST_SET_WINDOW_POS,
			.size = sizeof(request),
		},
		.id = window,
		.x = x,
		.y = y,
	};

	return twm_send_request((twm_request_t*)&request);
}

int twm_redraw_window(twm_window_t window, long x, long y, long width, long height) {
	twm_request_redraw_window_t request = {
		.base = {
			.type = TWM_REQUEST_REDRAW_WINDOW,
			.size = sizeof(request),
		},
		.id = window,
		.x = x,
		.y = y,
		.width  = width,
		.height = height,
	};
	
	return twm_send_request((twm_request_t*)&request);
}
int twm_start_dragging(twm_window_t window, long offset_x, long offset_y) {
	twm_request_start_dragging_t request = {
		.base = {
			.type = TWM_REQUEST_START_DRAGGING,
			.size = sizeof(request),
		},
		.id = window,
		.offset_x = offset_x,
		.offset_y = offset_y,
	};
	
	return twm_send_request((twm_request_t*)&request);
}

int twm_get_screen_fb(twm_screen_t screen, twm_fb_info_t *fb_info) {
	twm_request_get_screen_fb_t request = {
		.base = {
			.type = TWM_REQUEST_GET_SCREEN_FB,
			.size = sizeof(request),
		},
		.id = screen,
	};
	
	int ret = twm_send_request((twm_request_t*)&request);
	if (ret < 0) return ret;

	twm_event_screen_fb_t *event = wait_for_response(request.base.id);
	*fb_info = event->fb_info;
	free(event);
	return 0;
}

int twm_grab_desktop_hook(void) {
	twm_request_grab_desktop_hook_t request = {
		.base = {
			.type = TWM_REQUEST_GRAB_DESKTOP_HOOK,
			.size = sizeof(request),
		},
	};
	
	return twm_send_request((twm_request_t*)&request);
}
