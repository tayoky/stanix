#include <stdio.h>
#include <stdlib.h>
#include <gfx.h>
#include <twm.h>

int main() {
	twm_init(NULL);
	twm_window_t window = twm_create_window("test window", 250, 250, TWM_NULL);
	printf("created window with id %d\n", window);
	gfx_t *gfx = twm_get_window_gfx(window);
	printf("got window framebuffer at %p\n", gfx->framebuffer);
	gfx_clear(gfx, gfx_color(gfx, 0, 0,0 ));
	gfx_draw_rect(gfx, gfx_color(gfx, 255, 0, 0), 0, 0, 50, 50);
	twm_redraw_window(window, 0, 0, TWM_WHOLE_WIDTH, TWM_WHOLE_HEIGHT);
	for (;;) {
		twm_event_input_t *event = (twm_event_input_t*)twm_poll_event();
		if (event->base.type == TWM_EVENT_INPUT && event->type == TWM_INPUT_KEY) break;
		free(event);
	}
	gfx_free(gfx);
	twm_destroy_window(window);
}