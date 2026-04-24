
#include <gfx.h>
#include <stdlib.h>

int gfx_bound_check(gfx_t *gfx, long *x, long *y, long *width, long *height) {
	if (*x < 0) {
		*width += *x;
		*x = 0;
	}
	if (*x + *width >= gfx->width) {
		if (*x >= gfx->width) {
			return 0;
		}
		*width = gfx->width - *x;
	}
	if (*y < 0) {
		*height += *y;
		*y = 0;
	}
	if (*y + *height >= gfx->height) {
		if (*y >= gfx->height) {
			return 0;
		}
		*height = gfx->height - *y;
	}
    return 1;
}

gfx_t *gfx_create_clip(gfx_t *gfx, long x, long y, long width, long height) {
	if (!gfx) return NULL;
	gfx_t *clip = malloc(sizeof(gfx_t));
	*clip = *gfx;
	clip->buffer = (void*)gfx_pixel_addr(gfx, x, y);
	clip->width  = width;
	clip->height = height;
	clip->is_clip = 1;
	return clip;
}
