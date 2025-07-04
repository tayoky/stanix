#ifndef _GFX_H
#define _GFX_H

#include <sys/fb.h>
#include <stdint.h>

typedef struct gfx_context {
	void *framebuffer;
	void *backbuffer;
	long width;
	long height;
	long pitch;
	int bpp;
	int red_mask_shift;
	int red_mask_size;
	int green_mask_shift;
	int green_mask_size;
	int blue_mask_shift;
	int blue_mask_size;
} gfx_t;

typedef uint32_t color_t;

gfx_t *gfx_open_framebuffer(const char *path);
gfx_t *gfx_create(void *framebuffer,struct fb *);
void gfx_free(gfx_t *gfx);
void gfx_push_buffer(gfx_t *gfx);
color_t gfx_color(gfx_t *gfx,uint8_t r,uint8_t g,uint8_t b);

void gfx_draw_pixel(gfx_t *gfx,color_t color,long x,long y);
void gfx_draw_rect(gfx_t *gfx,color_t color,long x,long y,long width,long height);
void gfx_clear(gfx_t *gfx,color_t color);

#endif