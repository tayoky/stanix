#include <stdint.h>
#include "gfx.h"

color_t gfx_color(gfx_t *gfx,uint8_t r,uint8_t g,uint8_t b){
	uint32_t rs = ((uint32_t)r) >> (8 - gfx->red_mask_size);
	uint32_t gs = ((uint32_t)g) >> (8 - gfx->green_mask_size);
	uint32_t bs = ((uint32_t)b) >> (8 - gfx->blue_mask_size);

	return rs << gfx->red_mask_shift | gs << gfx->green_mask_shift | bs << gfx->blue_mask_shift;
}


color_t gfx_color_rgba(gfx_t *gfx,uint8_t r,uint8_t g,uint8_t b,uint8_t a) {
	return gfx_color(gfx, r, g, b) | (a << 24);
}
