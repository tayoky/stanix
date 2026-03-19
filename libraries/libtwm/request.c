#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
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

twm_window_t twm_create_window(const char *title, long width, long height) {
	twm_request_create_window_t request = {
		.base = {
			.type = TWM_REQUEST_CREATE_WINDOW,
			.size = sizeof(request),
		},
		.width  = width,
		.height = height,
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

	twm_event_window_fb_t *event = wait_for_response(request.base.id);

	*fb_info = event->fb_info;

	// we need to open the framebuffer
	*fd = open(event->path, O_WRONLY);
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

long twm_get_window_attr(twm_window_t window) {
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
	long attr = event->attr;
	free(event);
	return attr;
}

