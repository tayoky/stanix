#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <twm.h>

extern twm_ctx_t ctx;

static twm_handler_t handlers[TWM_EVENT_COUNT];
static void* handlers_data[TWM_EVENT_COUNT];

void twm_set_handler(int event_type, twm_handler_t handler, void *data) {
	if (event_type < 0 || event_type >= TWM_EVENT_COUNT) return;
	handlers[event_type] = handler;
	handlers_data[event_type] = data;
}

twm_event_t *twm_poll_event(void) {
	twm_event_t event;
	if (recv(ctx.fd, &event, sizeof(twm_event_t), 0) < 0) return NULL;
	char *buf = malloc(event.size);
	memcpy(buf, &event, sizeof(twm_event_t));
	recv(ctx.fd, buf + sizeof(twm_event_t), event.size - sizeof(twm_event_t), 0);

	return (twm_event_t*)buf;
}

void twm_handle_event(twm_event_t *event) {
	if (event->type < 0 || event->type >= TWM_EVENT_COUNT) return;
	if (handlers[event->type]) {
		handlers[event->type](event, handlers_data[event->type]);
	}
}