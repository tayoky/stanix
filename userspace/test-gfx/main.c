#include <stdio.h>
#include <gfx.h>

int main(){
	gfx_t *gfx = gfx_open_framebuffer("/dev/fb0");

	gfx_clear(gfx,gfx_color(gfx,0,0,0));
	gfx_draw_rect(gfx,gfx_color(gfx,0,255,0),0,0,200,200);
	gfx_push_buffer(gfx);

	getchar();

	gfx_free(gfx);
	return 0;	
}