#include <stdint.h>
#include "gfx.h"

void gfx_draw_pixel(gfx_t *gfx,color_t color,long x,long y){
	*(color_t *)((uintptr_t)gfx->backbuffer +  x * (gfx->bpp / 8) + y * gfx->pitch) = color;
}
