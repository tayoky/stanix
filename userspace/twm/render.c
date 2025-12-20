#include <twm.h>
#include <twm-internal.h>
#include <gfx.h>
#include <string.h>

void render_window_decor(window_t *window) {
	gfx_draw_rect(gfx, theme.primary, window->x + theme.border_width, window->y + theme.border_width, 
		window->width, theme.titlebar_height);
	gfx_draw_string(gfx, font, theme.font_color, window->x + theme.border_width, window->y + theme.border_width, window->title);
}

void render_window_content(window_t *window) {
	if (!window->fb_addr) {
		gfx_draw_rect(gfx, 0,  window->x + theme.border_width, window->y + theme.border_width + theme.titlebar_height, window->width, window->height);
		return;
	}
	uintptr_t dest_ptr = gfx_pixel_addr(gfx, window->x + theme.border_width, window->y + theme.border_width + theme.titlebar_height);
	uintptr_t src_ptr = window->fb_addr;
	size_t win_pitch = window->width * (gfx->bpp/8);
	for (long i=0; i<window->height; i++) {
		memcpy((void*)dest_ptr, (void*)src_ptr, win_pitch);
		src_ptr += win_pitch;
		dest_ptr += gfx->pitch;
	}
}