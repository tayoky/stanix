#include <stdint.h>
#include <math.h>
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

void gfx_draw_rounded_rect(gfx_t *gfx, color_t color, long x, long y, long width, long height, char corners, long rayon) {
	if (!gfx_bound_check(gfx, &x, &y, &width, &height)) {
		return;
	}
	for (int i = 0;i < rayon;i++) {
		long line_x = x;
		long line_width = width;
		long vertical_distance = rayon - i;
		long horizontal_distance = sqrt(rayon * rayon - vertical_distance * vertical_distance);
		long extend = rayon - horizontal_distance;

		if (corners & GFX_CORNER_TOP_LEFT) {
			line_x += extend;
			line_width -= extend;
		}
		if (corners & GFX_CORNER_TOP_RIGHT) {
			line_width -= extend;
		}
		uintptr_t ptr = gfx_pixel_addr(gfx, line_x, y);
		for (long j = 0;j < line_width;j++) {
			*(color_t *)ptr = color;
			ptr += gfx->bpp / 8;
		}
		y++;
	}
	for (int i = rayon;i < height-rayon;i++) {
		uintptr_t ptr = gfx_pixel_addr(gfx, x, y);
		for (long j = 0;j < width;j++) {
			*(color_t *)ptr = color;
			ptr += gfx->bpp / 8;
		}
		y++;
	}
	for (int i = 0;i < rayon;i++) {
		long line_x = x;
		long line_width = width;
		long vertical_distance = i+1;
		long horizontal_distance = sqrt(rayon * rayon - vertical_distance * vertical_distance);
		long extend = rayon - horizontal_distance;

		if (corners & GFX_CORNER_BOTTOM_LEFT) {
			line_x += extend;
			line_width -= extend;
		}
		if (corners & GFX_CORNER_BOTTOM_RIGHT) {
			line_width -= extend;
		}
		uintptr_t ptr = gfx_pixel_addr(gfx, line_x, y);
		for (long j = 0;j < line_width;j++) {
			*(color_t *)ptr = color;
			ptr += gfx->bpp / 8;
		}
		y++;
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
