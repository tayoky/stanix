#include "gfx.h"
#include "gfx-internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

texture_t *gfx_load_texture(gfx_t *gfx,const char *path){
	FILE *file = fopen(path,"r");
	if(!file){
		return NULL;
	}
	char magic[4];
	if(!fread(magic,sizeof(magic),1,file)){
		fclose(file);
		return NULL;
	}
	texture_t *texture = malloc(sizeof(texture_t));
	memset(texture,0,sizeof(texture_t));
	int ret;
	if(!memcmp(magic,"BM",2)){
		//bitmap
		ret = bmp_load(gfx,texture,file);
	} if(!memcmp(magic,"qoif",4)){
		ret = qoi_load(gfx,texture,file);
	} else {
		errno = EILSEQ; //not 100 %sure
		ret = -1;
	}
	if(ret < 0){
		free(texture);
		fclose(file);
		return NULL;
	}
	fclose(file);
	return texture;
}

void gfx_draw_texture(gfx_t *gfx,texture_t *texture,long x,long y){
	for(size_t tx=0; tx<texture->width; tx++){
		for (size_t ty = 0; ty < texture->height; ty++){
			gfx_draw_pixel(gfx,texture->bitmap[tx + ty * texture->width],tx + x,ty + y);	
		}
	}
}

void gfx_draw_texture_alpha(gfx_t *gfx,texture_t *texture,long x,long y){
	// TODO : actual transparency
	for(size_t tx=0; tx<texture->width; tx++){
		for (size_t ty = 0; ty < texture->height; ty++){
			if (texture->bitmap[tx + ty * texture->width] & 0xFF000000) {
				gfx_draw_pixel(gfx,texture->bitmap[tx + ty * texture->width],tx + x,ty + y);
			}
		}
	}
}

void gfx_draw_texture_scale(gfx_t *gfx,texture_t *texture,long x,long y,float scale_x,float scale_y){
	size_t rx = 0;
	scale_x = 1 / scale_x;
	scale_y = 1 / scale_y;
	for(float tx=0; tx<texture->width; tx += scale_x){
		size_t ry = 0;
		for (float ty = 0; ty < texture->height; ty += scale_y){
			long cx = tx;
			long cy = ty;
			gfx_draw_pixel(gfx,texture->bitmap[cx + cy * texture->width],rx + x,ry + y);
			ry++;	
		}
		rx++;
	}
}