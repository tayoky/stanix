#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <twm.h>

typedef struct twm_putback_event {
	twm_event_t *event;
	struct twm_putback_event *next;
} twm_putback_event_t;

extern twm_ctx_t ctx;

static twm_putback_event_t *putback_first = NULL;
static twm_putback_event_t *putback_last  = NULL;

twm_event_t *twm_raw_poll_event(void) {
	twm_event_t event;
	if (recv(ctx.fd, &event, sizeof(twm_event_t), 0) < 0) return NULL;
	char *buf = malloc(event.size);
	memcpy(buf, &event, sizeof(twm_event_t));
	recv(ctx.fd, buf + sizeof(twm_event_t), event.size - sizeof(twm_event_t), 0);

	return (twm_event_t*)buf;
}

twm_event_t *twm_raw_peek_event(void) {
	struct pollfd pollfd = {
		.events = POLLIN,
		.fd = ctx.fd,
	};
	poll(&pollfd, 1, 0);
	if (pollfd.revents & POLLIN) {
		return twm_poll_event();
	} else {
		return NULL;
	}
}

void twm_putback_event(twm_event_t *event) {
	twm_putback_event_t *putback_event = malloc(sizeof(twm_putback_event_t));
	putback_event->next  = NULL;
	putback_event->event = event;
	if (putback_last) {
		putback_last->next = putback_event;
	} else {
		putback_first = putback_event;
	}
	putback_last = putback_event;
}

twm_event_t *twm_peek_putback_event(void) {
	if (!putback_first) return NULL;
	twm_putback_event_t *putback_event = putback_first;
	putback_first = putback_event->next;
	if (putback_event == putback_last) {
		putback_last = NULL;
	}
	twm_event_t *event = putback_event->event;
	free(putback_event);
	return event;
}

twm_event_t *twm_poll_event(void) {
	twm_event_t *event = twm_peek_putback_event();
	if (event) return event;
	return twm_raw_poll_event();
}

twm_event_t *twm_peek_event(void) {
	twm_event_t *event = twm_peek_putback_event();
	if (event) return event;
	return twm_raw_peek_event();
}
