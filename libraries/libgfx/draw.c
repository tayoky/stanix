#include <stdint.h>
#include "gfx.h"

#undef gfx_draw_pixel

void gfx_draw_pixel(gfx_t *gfx,color_t color,long x,long y){
	*(color_t *)((uintptr_t)gfx->buffer +  x * (gfx->bpp / 8) + y * gfx->pitch) = color;
}


void gfx_draw_rect(gfx_t *gfx,color_t color,long x,long y,long width,long height){
	for(int i = 0;i < height;i++){
		uintptr_t ptr = (uintptr_t)gfx->buffer + x * (gfx->bpp / 8) + y *gfx->pitch;
		for(long j = 0;j < width;j++){
			*(color_t *)ptr = color;
			ptr += gfx->bpp / 8;
		}
		y++;
	}
}


void gfx_clear(gfx_t *gfx,color_t color){
	return gfx_draw_rect(gfx,color,0,0,gfx->width,gfx->height);
}
