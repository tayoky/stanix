#include <stdio.h>
#include <stdlib.h>
#include "gfx.h"
#include "gfx-internal.h"

static int (*font_types[])(font_t*,const char *path) = {
	psf1_load
};

font_t *gfx_load_font(const char *path){
	font_t *font = malloc(sizeof(font_t));

	for(size_t i = 0; i<sizeof(font_types) / sizeof(*font_types); i++){
		if(font_types[i](font,path) >= 0){
			return font;
		}
	}
	free(font);
	return NULL;
}

void gfx_free_font(font_t *font){
	if(font->free){
		font->free(font);
	}
	free(font);
}

void gfx_draw_char(gfx_t *gfx,font_t *font,color_t back,color_t front,long x,long y,int c){
	return font->draw_char(gfx,font,back,front,x,y,c);
}

void gfx_draw_string(gfx_t *gfx,font_t *font,color_t back,color_t front,long x,long y,const char *str){
	while(*str){
		gfx_draw_char(gfx,font,back,front,x,y,*(unsigned char *)str);
		x += gfx_char_width(font,*(unsigned char *)str);
		str++;
	}
}

long gfx_char_width(font_t *font,int c){
	return font->char_width(font,c);
}

long gfx_char_height(font_t *font,int c){
	return font->char_height(font,c);
}
