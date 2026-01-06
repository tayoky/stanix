#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <twm.h>

extern twm_ctx_t ctx;

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

	twm_event_window_created_t *event;
	for (;;) {
		event = (twm_event_window_created_t*)twm_poll_event();
		if (!event) return -1;
		if (event->base.request_id != request.base.id) {
			twm_handle_event((twm_event_t*)event);
			continue;
		}
		break;
	}
	// we got the event !
	return event->id;
}
