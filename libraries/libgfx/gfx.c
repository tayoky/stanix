#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/fb.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gfx.h"

gfx_t *gfx_open_framebuffer(const char *path){
	int fb = open(path,O_WRONLY);
	if(fb < 0)return NULL;

	//get framebuffer info
	struct fb fb_info;
	if(ioctl(fb,IOCTL_GET_FB_INFO,&fb_info) < 0)return NULL;

	//map framebuffer
	void *framebuffrer = mmap(NULL,fb_info.pitch * fb_info.height,PROT_WRITE,MAP_SHARED,fb,0);
	if(framebuffrer == map_failed)return NULL;

	gfx_t *gfx = gfx_create(framebuffrer,&fb_info);
	close(fb);
	//TODO : unmap framebuffer on fail
	return gfx;
}

gfx_t *gfx_create(void *framebuffer,struct fb *fb_info){
	gfx_t *gfx = malloc(sizeof(gfx_t));
	if(!gfx)return NULL;

	gfx->backbuffer = malloc(fb_info->height * fb_info->pitch);
	gfx->buffer = gfx->backbuffer;
	if(!gfx->backbuffer){
		free(gfx);
		return NULL;
	}

	gfx->framebuffer = framebuffer;
	gfx->bpp = fb_info->bpp;
	gfx->red_mask_shift   = fb_info->red_mask_shift;
	gfx->red_mask_size    = fb_info->red_mask_size;
	gfx->green_mask_shift = fb_info->green_mask_shift;
	gfx->green_mask_size  = fb_info->green_mask_size;
	gfx->blue_mask_shift  = fb_info->blue_mask_shift;
	gfx->blue_mask_size   = fb_info->blue_mask_size;
	gfx->width = fb_info->width;
	gfx->height = fb_info->height;
	gfx->pitch = fb_info->pitch;

	return gfx;
}

void gfx_free(gfx_t *gfx){
	//TODO : ummap framebuffer
	free(gfx->backbuffer);
	free(gfx);
}


void gfx_push_buffer(gfx_t *gfx){
	if(gfx->backbuffer)
	memcpy(gfx->framebuffer,gfx->backbuffer,gfx->height * gfx->pitch);
}

void gfx_push_rect(gfx_t *gfx,long x,long y,long width,long height){
	if(!gfx->backbuffer)return;
	for(int i = 0;i < height;i++){
		uintptr_t ptr =  x * (gfx->bpp / 8) + y *gfx->pitch;
		memcpy((char *)gfx->framebuffer + ptr,(char *)gfx->backbuffer + ptr,width * gfx->bpp / 8);
		y++;
	}
}

void gfx_enable_backbuffer(gfx_t *gfx){
	if(gfx->backbuffer)return;
	gfx->backbuffer = malloc(gfx->height * gfx->pitch);
	gfx->buffer = gfx->backbuffer;
}

void gfx_disable_backbuffer(gfx_t *gfx){
	free(gfx->backbuffer);
	gfx->backbuffer = NULL;
	gfx->buffer = gfx->framebuffer;
}