#include "gfx.h"
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

struct __attribute__((packed)) bmp_header {
	char magic[2];
	uint32_t size;
	uint32_t reserved;
	uint32_t offset;
};

struct __attribute__((packed)) bmp_info_header {
	uint32_t size;
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t bpp;
	uint32_t compression;
	uint32_t image_size;
	uint32_t xpm;
	uint32_t ypm;
	uint32_t colors;
	uint32_t imp;
};

#define BI_RGB 0

int bmp_load(gfx_t *gfx,texture_t *texture,FILE *file){
	rewind(file);
	struct bmp_header header;
	struct bmp_info_header info_header;
	if(!fread(&header,sizeof(header),1,file)){
		errno = EILSEQ;
		return -1;
	}
	if(!fread(&info_header,sizeof(info_header),1,file)){
		errno = EILSEQ;
		return -1;
	}

	if(info_header.size < sizeof(info_header) || info_header.planes != 1){
		errno = EILSEQ;
		return -1;
	}

	if(info_header.compression != BI_RGB){
		//TODO : rle support?
		errno = EILSEQ;
		return -1;
	}

	texture->width  = info_header.width;
	texture->height = info_header.height;

	//color table ?
	color_t *color_table = NULL;
	if(info_header.bpp <= 8){
		//yes color table
		size_t num_colors;
		switch(info_header.bpp){
		case 8:
			num_colors = 256;
			break;
		case 4:
			num_colors = 16;
			break;
		case 1:
			num_colors = 2;
			break;
		default:
			//invalid
			errno = EILSEQ;
			return -1;
		}
		color_table = malloc(sizeof(color_t) * num_colors);
		if(!color_table)return -1;
		for (size_t i = 0; i < num_colors; i++){
			uint8_t bgra[4];
			fread(bgra,sizeof(bgra),1,file);
			color_table[i] = gfx_color(gfx,bgra[2],bgra[1],bgra[0]);
		}
	}

	fseek(file,header.offset,SEEK_SET);
	texture->bitmap = malloc(sizeof(color_t) * texture->width * texture->height);
	for(size_t y=0; y<texture->height; y++){
		size_t row_size = 0;
		for(size_t x=0; x<texture->width; x++){
			if(info_header.bpp == 24 || info_header.bpp == 32){
				// no color table
				uint8_t rgba[4];
				if(info_header.bpp == 32){
					fread(&rgba[3],sizeof(uint8_t),1,file);
				}
				fread(rgba,sizeof(uint8_t),3,file);
				uint8_t tmp = rgba[0];
				rgba[0] = rgba[2];
				rgba[2] = tmp;
				texture->bitmap[(texture->height - y - 1) * texture->width + x] = gfx_color(gfx,rgba[0],rgba[1],rgba[2]);
				row_size += info_header.bpp / CHAR_BIT;
			} else {
				//TODO
				uint8_t byte;
				fread(&byte,sizeof(byte),1,file);
				uint8_t mask = ((uint16_t)((1 << info_header.bpp) - 1));
				for(size_t i=0; i<CHAR_BIT/info_header.bpp; i++){
					uint8_t index = (byte >> ((CHAR_BIT/info_header.bpp - i - 1) * info_header.bpp)) & mask;
					
					texture->bitmap[(texture->height - y - 1) * texture->width + x + i] = color_table[index];
				}
				row_size++;
				x += CHAR_BIT/info_header.bpp - 1;
			}
		}
		if(row_size%4)fseek(file,4-(row_size%4),SEEK_CUR);
	}

	if(color_table)free(color_table);

	return 0;
}