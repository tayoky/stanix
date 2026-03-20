#include <stdio.h>
#include <gfx.h>
#include <twm.h>

int main() {
	twm_init(NULL);
	twm_window_t window = twm_create_window("test window", 250, 250);
	printf("created window with id %d\n", window);
	gfx_t *gfx = twm_get_window_gfx(window);
	printf("got window framebuffer at %p\n", gfx->framebuffer);
	gfx_clear(gfx, gfx_color(gfx, 0, 0,0 ));
	gfx_draw_rect(gfx, gfx_color(gfx, 255, 0, 0), 0, 0, 50, 50);
	twm_redraw_window(window, 0, 0, TWM_WHOLE_WIDTH, TWM_WHOLE_HEIGHT);
	for(;;);
}