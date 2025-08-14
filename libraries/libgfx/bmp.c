#include "gfx.h"
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
	fseek(file,header.offset,SEEK_SET);
	texture->bitmap = malloc(sizeof(color_t) * texture->width * texture->height);
	for(size_t y=0; y<texture->height; y++){
		size_t row_size = 0; // in bits
		for(size_t x=0; x<texture->width; x++){
			uint8_t rgb[3];
			if(info_header.bpp >= 24){
				// no color pallete
				//TODO : padding ?
				fread(rgb,sizeof(rgb),1,file);
			} else {
				//TODO
			}
			texture->bitmap[x + y * texture->width] = gfx_color(gfx,rgb[0],rgb[1],rgb[2]);
			row_size += info_header.bpp;
		}
		row_size /= 8;
		if(row_size%4)fseek(file,4-(row_size%4),SEEK_CUR);
	}

	return 0;
}