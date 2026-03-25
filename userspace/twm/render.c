#include <twm.h>
#include <twm-internal.h>
#include <gfx.h>
#include <string.h>

static long invalidate_start_x = LONG_MAX;
static long invalidate_start_y = LONG_MAX;
static long invalidate_end_x = 0;
static long invalidate_end_y = 0;

static void render_window_decor(window_t *window) {
	long titlebar_y = window->y - theme.border_width - theme.titlebar_height;
	long border_x = window->x - theme.border_width;
	long border_y = titlebar_y - theme.border_width;
	long border_width = window->width + theme.border_width;
	long border_height = window->height + 2 * theme.border_width + theme.titlebar_height;

	// titlebar
	gfx_draw_rect(gfx, theme.primary, window->x, titlebar_y,
		window->width, theme.titlebar_height);
	gfx_draw_string(gfx, font, theme.font_color, window->x, titlebar_y, window->title);

	// main border
	gfx_draw_wire_rect(gfx, theme.secondary, border_x, border_y, border_width, border_height, theme.border_width);

	// middle border
	gfx_draw_rect(gfx, theme.secondary, window->x, window->y - theme.border_width, window->width, theme.border_width);	
}

static void render_window_content(window_t *window) {
	uintptr_t dest_ptr = gfx_pixel_addr(gfx, window->x, window->y);
	uintptr_t src_ptr = (uintptr_t)window->framebuffer;
	size_t win_pitch = window->width * (gfx->bpp/8);
	for (long i=0; i<window->height; i++) {
		memcpy((void*)dest_ptr, (void*)src_ptr, win_pitch);
		src_ptr += win_pitch;
		dest_ptr += gfx->pitch;
	}
}

static void render_cursor(cursor_t *cursor) {
	gfx_draw_texture_alpha(gfx, theme.cursor_texture, cursor->x, cursor->y);
}


void move_cursor(cursor_t *cursor, long new_x, long new_y) {
	invalidate_rect(cursor->x, cursor->y, theme.cursor_texture->width, theme.cursor_texture->height);
	cursor->x = new_x;
	cursor->y = new_y;
	invalidate_rect(cursor->x, cursor->y, theme.cursor_texture->width, theme.cursor_texture->height);
}

void invalidate_rect(long x, long y, long width, long height) {
	long end_x = x + width;
	long end_y = y + height;

	if (x < invalidate_start_x) invalidate_start_x = x;
	if (y < invalidate_start_y) invalidate_start_y = y;
	if (end_x > invalidate_end_x) invalidate_end_x = end_x;
	if (end_y > invalidate_end_y) invalidate_end_y = end_y;
}

void render(void) {
	if (invalidate_end_x == 0 && invalidate_end_y == 0) return;
	long invalidate_width = invalidate_end_x - invalidate_start_x;
	long invalidate_height = invalidate_end_y - invalidate_start_y;

	// we need to render in this order
	// - background
	// - windows
	// - cursor

	// what an amazing background
	gfx_draw_rect(gfx, gfx_color(gfx, 0, 0, 0), invalidate_start_x, invalidate_start_y, invalidate_width, invalidate_height);

	for (window_t *current=window_stack_bottom; current; current = current->next) {
		// TODO : rerender only if it is overlapping
		render_window_content(current);
		render_window_decor(current);
	}

	if (invalidate_start_x < cursor.x + (long)theme.cursor_texture->width 
		&& invalidate_start_y < cursor.y + (long)theme.cursor_texture->height
		&& invalidate_end_x > cursor.x
		&& invalidate_end_y > cursor.y) {
		render_cursor(&cursor);
	}

	gfx_push_rect(gfx, invalidate_start_x, invalidate_start_y, invalidate_width, invalidate_height);

	invalidate_start_x = LONG_MAX;
	invalidate_start_y = LONG_MAX;
	invalidate_end_x = 0;
	invalidate_end_y = 0;
}
