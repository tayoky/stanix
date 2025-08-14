#include <stdio.h>
#include <gfx.h>

int main(int argc,char **argv){
	gfx_t *gfx = gfx_open_framebuffer("/dev/fb0");

	gfx_clear(gfx,gfx_color(gfx,0,0,0));
	texture_t *tex = gfx_load_texture(gfx,argv[1]);
	gfx_draw_texture(gfx,tex,0,0);
	gfx_push_buffer(gfx);

	getchar();

	gfx_free(gfx);
	return 0;	
}