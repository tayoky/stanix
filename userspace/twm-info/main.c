#include <stdio.h>
#include <stdlib.h>
#include <input.h>
#include <gfx.h>
#include <twm.h>

void handle_screen(twm_event_screen_t *event) {
	twm_screen_attr_t screen;
	twm_get_screen_attr(event->screen, &screen);
	printf("screen %d\n", event->screen);
	printf("\tname : '%s'\n", screen.name);
	printf("\res   : %ldx%ld\n", screen.fb_info.width, screen.fb_info.height);
	printf("\tbpp  : %d\n", screen.fb_info.bpp);
}

int main() {
	if (twm_init(NULL) < 0) {
		fprintf(stderr, "failed to connect to twm\n");
		return 1;
	}

	twm_event_t *event;
	int first = 1;
	while ((event = (first ? twm_poll_event() : twm_peek_event()))) {
		first = 0;
		switch (event->type) {
		case TWM_EVENT_SCREEN_ADDED:
			handle_screen((twm_event_screen_t*)event);
			break;
		}
		free(event);
	}

	twm_fini();
}