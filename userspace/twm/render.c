#include <twm.h>
#include <twm-internal.h>
#include <gfx.h>
#include <string.h>

#define min(a, b) (a < b) ? (a) : (b)

static void render_window_decor(screen_t *screen, window_t *window) {
	long win_x, win_y, win_width, win_height;
	window_get_bounds(window, &win_x, &win_y, &win_width, &win_height);
	win_x -= screen->x;
	win_y -= screen->y;
	long titlebar_x = win_x + theme.border_width;
	long titlebar_y = win_y + theme.border_width;
	gfx_t *gfx = screen->gfx;
	gfx_draw_rect(gfx, gfx_color(gfx, 60, 141, 63), win_x, titlebar_y, win_width, theme.titlebar_height);
	gfx_draw_wire_rect(gfx, gfx_color(gfx, 44, 105, 47), win_x, win_y, win_width - theme.border_width, win_height- theme.border_width, theme.border_width);
	gfx_draw_rect(gfx, gfx_color(gfx, 44, 105, 47), win_x, win_y + theme.border_width + theme.titlebar_height, win_width, theme.border_width);
	gfx_draw_string(gfx, font, gfx_color(gfx, 0, 0, 0), titlebar_x + 2 * theme.padding, titlebar_y + 2 * theme.padding, window->title);
}

static void render_window_content(screen_t *screen, window_t *window) {
	long win_x, win_y, win_width, win_height;
	window_get_inner_bounds(window, &win_x, &win_y, &win_width, &win_height);
	win_x -= screen->x;
	win_y -= screen->y;
	gfx_t *gfx = screen->gfx;
	gfx_draw_rect(gfx, gfx_color(gfx, 0, 0, 0), win_x, win_y, win_width, win_height);

	if (window->framebuffer) {
		long y = win_y;
		long x = win_x;
		long width  = min(win_width , window->fb_info.width);
		long height = min(win_height, window->fb_info.height);
		if (!gfx_bound_check(gfx, &x, &y, &width, &height)) return;

		uintptr_t dest_ptr = gfx_pixel_addr(gfx, x, y);
		size_t win_pitch = window->fb_info.pitch;
		uintptr_t src_ptr = (uintptr_t)window->framebuffer + (x - win_x) * (gfx->bpp/8) + (y - win_y) * win_pitch;
		size_t copy_width = width * (gfx->bpp/8);
		
		for (long i=0; i<height; i++) {
			memcpy((void*)dest_ptr, (void*)src_ptr, copy_width);
			src_ptr += win_pitch;
			dest_ptr += gfx->pitch;
		}
	}
}

static void render_cursor(screen_t *screen, cursor_t *cursor) {
	gfx_draw_texture_alpha(screen->gfx, theme.cursor_texture, cursor->x, cursor->y);
}

void move_cursor(cursor_t *cursor, long new_x, long new_y) {
	invalidate_rect(cursor->x, cursor->y, theme.cursor_texture->width, theme.cursor_texture->height);
	cursor->x = new_x;
	cursor->y = new_y;
	invalidate_rect(cursor->x, cursor->y, theme.cursor_texture->width, theme.cursor_texture->height);
}

static void invalidate_screen_rect(long x, long y, long width, long height) {
	long end_x = x + width;
	long end_y = y + height;

	if (x < screen->invalidate_start_x) screen->invalidate_start_x = x;
	if (y < screen->invalidate_start_y) screen->invalidate_start_y = y;
	if (end_x > screen->invalidate_end_x) screen->invalidate_end_x = end_x;
	if (end_y > screen->invalidate_end_y) screen->invalidate_end_y = end_y;
}

void invalidate_rect(long x, long y, long width, long height) {
	utils_hashmap_foreach(key, screen, &screens) {
		(void)id;
		if (is_inside_screen(screen, x, y, width, height)) {
			invalidate_screen_rect(screen, x, y, width, height);
		}
	}
}

static void render_screen(screen_t *screen) {
	gfx_t *gfx = screen->gfx;
	if (screen->invalidate_end_x == 0 && screen->invalidate_end_y == 0) return;
	if (screen->invalidate_start_x < screen->x) screen->invalidate_start_x = screen->x;
	if (screen->invalidate_start_y < screen->y) screen->invalidate_start_y = screen->y;
	if (screen->invalidate_end_x > screen->x + gfx->width) screen->invalidate_end_x = screen->x + gfx->width;
	if (screen->invalidate_end_y > screen->y + gfx->height) screen->invalidate_end_y = screen->y + gfx->height;
	
	long invalidate_width = screen->invalidate_end_x - screen->invalidate_start_x;
	long invalidate_height = screen->invalidate_end_y - screen->invalidate_start_y;

	// we need to render in this order
	// - background
	// - windows
	// - cursor

	// what an amazing background
	gfx_draw_rect(gfx, gfx_color(gfx, 0, 0, 0), screen->invalidate_start_x - screen->x, screen->invalidate_start_y - screen->y, invalidate_width, invalidate_height);


	for (int zindex = TWM_ZINDEX_MIN; zindex <= TWM_ZINDEX_MAX; zindex++) {
		utils_list_foreach(&window_stacks[zindex], node) {
			window_t *current = WINDOW_FROM_NODE(node);
			if (!is_inside_window(current, invalidate_start_x, invalidate_start_y, invalidate_width, invalidate_height)) continue;
			render_window_content(screen, current);
			if (current->attribute & TWM_ATTR_DECORED) {
				render_window_decor(screen, current);
			}
		}
	}

	if (screen->invalidate_start_x < cursor.x + (long)theme.cursor_texture->width 
		&& screen->invalidate_start_y < cursor.y + (long)theme.cursor_texture->height
		&& screen->invalidate_end_x > cursor.x
		&& screen->invalidate_end_y > cursor.y) {
		render_cursor(screen, &cursor);
	}

	gfx_push_rect(gfx, invalidate_start_x - screen->x, invalidate_start_y - screen->y, invalidate_width, invalidate_height);

	screen->invalidate_start_x = LONG_MAX;
	screen->invalidate_start_y = LONG_MAX;
	screen->invalidate_end_x = 0;
	screen->invalidate_end_y = 0;
}

void render(void) {
	utils_hashmap_foreach(key, screen, &screens) {
		(void)id;
		render_screen(screen);
	}
}
