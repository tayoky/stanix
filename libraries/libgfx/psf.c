#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "gfx.h"
#include "gfx-internal.h"

typedef struct {
    uint16_t magic; // Magic bytes for identification.
    uint8_t font_mode; // PSF font mode.
    uint8_t character_size; // PSF character size.
} PSF1_Header;

typedef struct {
	PSF1_Header header;
	size_t data_lenght;
	char *data;
} PSF1_data;

static void psf1_free(font_t *font){
	PSF1_data *data = font->private;
	free(data->data);
	free(data);
}

static long psf1_char_width(font_t *font,int c){
	(void)c;
	(void)font;
	return 8;
}

static long psf1_char_height(font_t *font,int c){
	(void)c;
	PSF1_data *data = font->private;
	return data->header.character_size;
}

static void psf1_draw_char(gfx_t *gfx,font_t *font,color_t back,color_t front,long x,long y,int c){
	PSF1_data *data = font->private;
	char *current_byte = &data->data[c * data->header.character_size];
	for (uint16_t i = 0; i < data->header.character_size; i++){
		for (uint8_t j = 0; j < 8; j++){
			if(((*current_byte) >> (7 - j)) & 0x01){
				gfx_draw_pixel(gfx,front,x + j,y + i);
			} else {
				gfx_draw_pixel(gfx,back,x + j,y + i);
			}
		}
		
		current_byte++;
	}
}

int psf1_load(font_t *font,const char *path){
	FILE *file = fopen(path,"r");
	if(!file)return -1;

	PSF1_Header header;
	fread(&header,sizeof(PSF1_Header),1,file);

	if(header.magic != 0x0436){
		errno = EINVAL;
		fclose(file);
		return -1;
	}
	
	PSF1_data *data = malloc(sizeof(PSF1_data));

	fseek(file,0,SEEK_END);
	data->data_lenght = ftell(file) - sizeof(PSF1_Header);
	
	data->data = malloc(data->data_lenght);
	fseek(file,sizeof(PSF1_Header),SEEK_SET);
	fread(data->data,data->data_lenght,1,file);

	fclose(file);

	data->header = header;

	font->private = data;
	font->char_height = psf1_char_height;
	font->char_width  = psf1_char_width;
	font->draw_char   = psf1_draw_char;
	font->free        = psf1_free;

	return 0;
}
