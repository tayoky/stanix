#include <twm.h>
#include <twm-internal.h>
#include <gfx.h>
#include <string.h>

void render_window_decor(window_t *window) {
	long titlebar_y = window->y - theme.border_width - theme.titlebar_height;
	long border_x = window->x - theme.border_width;
	long border_y = titlebar_y - theme.border_width;
	long border_width = window->width + theme.border_width;
	long border_height = window->height + 2 * theme.border_width + theme.titlebar_height;

	gfx_draw_rect(gfx, theme.primary, window->x, titlebar_y,
		window->width, theme.titlebar_height);
	gfx_draw_string(gfx, font, theme.font_color, window->x, titlebar_y, window->title);
	gfx_draw_wire_rect(gfx, theme.secondary, border_x, border_y, border_width, border_height, theme.border_width);
	gfx_push_rect(gfx, border_x, border_y, border_width + theme.border_width, border_height + theme.border_width);
}

void render_window_content(window_t *window) {
	if (!window->fb_addr) {
		gfx_draw_rect(gfx, 0,  window->x, window->y + theme.border_width + theme.titlebar_height, window->width, window->height);
		return;
	}
	uintptr_t dest_ptr = gfx_pixel_addr(gfx, window->x, window->y);
	uintptr_t src_ptr = window->fb_addr;
	size_t win_pitch = window->width * (gfx->bpp/8);
	for (long i=0; i<window->height; i++) {
		memcpy((void*)dest_ptr, (void*)src_ptr, win_pitch);
		src_ptr += win_pitch;
		dest_ptr += gfx->pitch;
	}
}