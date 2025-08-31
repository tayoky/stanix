#include <stdio.h>
#include <gfx.h>

int main(int argc,char **argv){
	if(argc < 2){
		printf("usage : test-gfx IMAGE\n");
		return 1;
	}
	gfx_t *gfx = gfx_open_framebuffer("/dev/fb0");

	gfx_clear(gfx,gfx_color(gfx,0,0,0));
	texture_t *tex = gfx_load_texture(gfx,argv[1]);
	if(!tex){
		perror(argv[1]);
		return 1;
	}
	font_t *font = gfx_load_font("/zap-light16.psf");
	gfx_draw_string(gfx,font,gfx_color(gfx,255,255,255),0,tex->height,argv[1]);
	gfx_draw_texture(gfx,tex,0,0);
	gfx_push_buffer(gfx);

	getchar();

	gfx_free(gfx);
	return 0;	
}