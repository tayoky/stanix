#include <sys/socket.h>
#include <twm.h>

extern twm_ctx_t ctx;

twm_event_t *twm_poll_event(void) {
	twm_event_t event;
	if (recv(ctx.fd, &event, sizeof(twm_event_t), 0) < 0) return NULL;
	char *buf = malloc(event.size);
	recv(ctx.fd, buf + sizeof(twm_event_t), event.size - sizeof(twm_event_t), 0);

	return buf;
}

void twm_handle_event(twm_event_t *event) {
	// TODO
	(void)event;
}