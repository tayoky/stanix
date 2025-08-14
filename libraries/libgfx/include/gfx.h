#ifndef _GFX_H
#define _GFX_H

#include <sys/fb.h>
#include <stdint.h>

typedef uint32_t color_t;

typedef struct gfx_context {
	void *framebuffer;
	void *backbuffer;
	void *buffer;
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

typedef struct gfx_font {
	int type;
	void *private;
	void (*draw_char)(gfx_t *,struct gfx_font*,color_t,long,long,int);
	long (*char_width)(struct gfx_font *,int c);
	long (*char_height)(struct gfx_font *,int c);
	void (*free)(struct gfx_font *);
} font_t;

typedef struct gfx_texture {
	size_t width;
	size_t height;
	color_t *bitmap;
} texture_t;

gfx_t *gfx_open_framebuffer(const char *path);
gfx_t *gfx_create(void *framebuffer,struct fb *);
void gfx_free(gfx_t *gfx);
void gfx_push_buffer(gfx_t *gfx);
void gfx_push_rect(gfx_t *gfx,long x,long y,long width,long height);
void gfx_enable_backbuffer(gfx_t *gfx);
void gfx_disable_backbuffer(gfx_t *gfx);
color_t gfx_color(gfx_t *gfx,uint8_t r,uint8_t g,uint8_t b);

void gfx_draw_pixel(gfx_t *gfx,color_t color,long x,long y);
void gfx_draw_rect(gfx_t *gfx,color_t color,long x,long y,long width,long height);
void gfx_clear(gfx_t *gfx,color_t color);

font_t *gfx_load_font(const char *path);
void gfx_free_font(font_t *font);
void gfx_draw_char(gfx_t *gfx,font_t *font,color_t color,long x,long y,int c);
void gfx_draw_string(gfx_t *gfx,font_t *font,color_t color,long x,long y,const char *str);
long gfx_char_width(font_t *font,int c);
long gfx_char_height(font_t *font,int c);


texture_t *gfx_load_texture(gfx_t *gfx,const char *path);
void gfx_draw_texture(gfx_t *gfx,texture_t *textutre,long x,long y);

#define gfx_draw_pixel(gfx,color,x,y) {*(color_t *)((uintptr_t)gfx->buffer +  (x) * (gfx->bpp / 8) + (y) * gfx->pitch) = (color);}

#endif