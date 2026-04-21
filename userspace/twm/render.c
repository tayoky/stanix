#include <twm.h>
#include <twm-internal.h>
#include <gfx.h>
#include <string.h>

static long invalidate_start_x = LONG_MAX;
static long invalidate_start_y = LONG_MAX;
static long invalidate_end_x = 0;
static long invalidate_end_y = 0;

static void render_window_content(window_t *window) {
	long win_x;
	long win_y;
	real_window_coord(window, &win_x, &win_y);
	long y = win_y;
	long x = win_x;
	long width  = window->width;
	long height = window->height;
	if (!gfx_bound_check(gfx, &x, &y, &width, &height)) return;

	uintptr_t dest_ptr = gfx_pixel_addr(gfx, x, y);
	uintptr_t src_ptr = (uintptr_t)window->framebuffer + (x - win_x) * (gfx->bpp/8);
	size_t win_pitch = window->width * (gfx->bpp/8);
	size_t copy_width = width * (gfx->bpp/8);
	
	for (long i=0; i<height; i++) {
		memcpy((void*)dest_ptr, (void*)src_ptr, copy_width);
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
	if (invalidate_start_x < 0) invalidate_start_x = 0;
	if (invalidate_start_y < 0) invalidate_start_y = 0;
	if (invalidate_end_x > gfx->width) invalidate_end_x = gfx->width;
	if (invalidate_end_y > gfx->height) invalidate_end_y = gfx->height;
	
	long invalidate_width = invalidate_end_x - invalidate_start_x;
	long invalidate_height = invalidate_end_y - invalidate_start_y;

	// we need to render in this order
	// - background
	// - windows
	// - cursor

	// what an amazing background
	gfx_draw_rect(gfx, gfx_color(gfx, 0, 0, 0), invalidate_start_x, invalidate_start_y, invalidate_width, invalidate_height);

	for (window_t *current=window_stack_bottom; current; current = current->next) {
		if (is_inside_window(current, invalidate_start_x, invalidate_start_y, invalidate_width, invalidate_height)) {
			render_window_content(current);
		}
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
