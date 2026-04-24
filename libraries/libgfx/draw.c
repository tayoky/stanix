#include <stdint.h>
#include <math.h>
#include <string.h>
#include "gfx.h"

#undef gfx_draw_pixel

void gfx_draw_pixel(gfx_t *gfx, color_t color, long x, long y) {
	*(color_t *)((uintptr_t)gfx->buffer + x * (gfx->bpp / 8) + y * gfx->pitch) = color;
}


void gfx_draw_rect(gfx_t *gfx, color_t color, long x, long y, long width, long height) {
	if (!gfx_bound_check(gfx, &x, &y, &width, &height)) {
		return;
	}
	for (int i = 0;i < height;i++) {
		uintptr_t ptr = (uintptr_t)gfx->buffer + x * (gfx->bpp / 8) + y * gfx->pitch;
		for (long j = 0;j < width;j++) {
			*(color_t *)ptr = color;
			ptr += gfx->bpp / 8;
		}
		y++;
	}
}

static void gfx_rounded_rect_get_line(long x, long y, long width, long height, char corners, long rayon, long *line_x, long *line_width) {
	if (y < rayon) {
		// top corners
		*line_x = x;
		*line_width = width;
		long vertical_distance = rayon - y;
		long horizontal_distance = sqrt(rayon * rayon - vertical_distance * vertical_distance);
		long extend = rayon - horizontal_distance;

		if (corners & GFX_CORNER_TOP_LEFT) {
			*line_x += extend;
			*line_width -= extend;
		}
		if (corners & GFX_CORNER_TOP_RIGHT) {
			*line_width -= extend;
		}

	} else if(y < height - rayon) {
		// middle
		*line_x = x;
		*line_width = width;
	} else {
		// bottom corners
		*line_x = x;
		*line_width = width;
		long vertical_distance = y + rayon - height +1;
		long horizontal_distance = sqrt(rayon * rayon - vertical_distance * vertical_distance);
		long extend = rayon - horizontal_distance;

		if (corners & GFX_CORNER_BOTTOM_LEFT) {
			*line_x += extend;
			*line_width -= extend;
		}
		if (corners & GFX_CORNER_BOTTOM_RIGHT) {
			*line_width -= extend;
		}
	}
}

void gfx_draw_rounded_rect(gfx_t *gfx, color_t color, long x, long y, long width, long height, char corners, long rayon) {
	long cur_y = 0;
	if (y < 0) {
		cur_y = -y;
	}
	for (; cur_y<height && y + cur_y <gfx->height; cur_y++) {
		long line_x;
		long line_width;
		gfx_rounded_rect_get_line(x, cur_y, width, height, corners, rayon, &line_x, &line_width);
		if (line_x < 0) {
			line_width += line_x;
			line_x = 0;
		}
		if (line_x + line_width > gfx->width) {
			line_width = gfx->width - line_x;
		}
		uintptr_t ptr = gfx_pixel_addr(gfx, line_x, cur_y + y);
		for (long j = 0;j < line_width;j++) {
			*(color_t *)ptr = color;
			ptr += gfx->bpp / 8;
		}
	}
}

void gfx_draw_wire_rect(gfx_t *gfx, color_t color, long x, long y, long width, long height, long border) {
	gfx_draw_rect(gfx, color, x, y, border, height);
	gfx_draw_rect(gfx, color, x + width, y, border, height);
	gfx_draw_rect(gfx, color, x + border, y, width - border, border);
	gfx_draw_rect(gfx, color, x + border, y + height, width - border, border);
}


void gfx_clear(gfx_t *gfx, color_t color) {
	return gfx_draw_rect(gfx, color, 0, 0, gfx->width, gfx->height);
}

void gfx_draw_buffer(gfx_t *gfx, long x, long y, gfx_t *buffer) {
	for (int i = 0;i < buffer->height;i++) {
		uintptr_t dest = gfx_pixel_addr(gfx, x, y);
		uintptr_t src  = gfx_pixel_addr(buffer, 0, i);
		memcpy((void*)dest, (void*)src, buffer->pitch);
		y++;
	}
}
