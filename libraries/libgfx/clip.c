
#include <gfx.h>

int gfx_bound_check(gfx_t *gfx, long *x, long *y, long *width, long *height) {
	if (*x < 0) {
		*width += *x;
		*x = 0;
	} else if (*x + *width >= gfx->width) {
		if (*x >= gfx->width) {
			return 0;
		}
		*width = gfx->width - *x;
	}
	if (*y < 0) {
		*height += *y;
		*y = 0;
	} else if (*y + *height >= gfx->height) {
		if (*y >= gfx->height) {
			return 0;
		}
		*height = gfx->height - *y;
	}
    return 1;
}
