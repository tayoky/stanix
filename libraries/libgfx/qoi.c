#include "gfx.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

struct qoi_pixel {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct qoi_header {
	char magic[4];
	uint32_t width;
	uint32_t height;
	uint8_t channels;
	uint8_t colorspace;
};

#define QOI_OP_INDEX 0b00
#define QOI_OP_DIFF  0b01
#define QOI_OP_LUMA  0b10
#define QOI_OP_RUN   0b11

#define QOI_OP_RGB   0b11111110
#define QOI_OP_RGBA  0b11111111

#define QOI_COLOR_HASH(px) (((px).r * 3 + (px).g * 5 + (px).b * 7 + (px).a * 11) & 63)

#define fix_endian(t) t = ((t >> 24) & 0xff) | ((t >> 8) & 0xff00) | ((t << 8) & 0xff0000) | ((t << 24) & 0xff000000)

int qoi_load(gfx_t *gfx,texture_t *texture,FILE *file){
	struct qoi_pixel prev = {.r = 0,.g = 0,.b = 0,.a = 255};
	struct qoi_pixel pixels_buf[64];
	memset(pixels_buf,0,sizeof(pixels_buf));

	rewind(file);
	struct qoi_header header;
	if(!fread(&header,sizeof(header),1,file))return -1;
	fix_endian(header.width);
	fix_endian(header.height);

	texture->width  = header.width;
	texture->height = header.height;
	texture->bitmap = malloc(header.width * header.height * sizeof(color_t));

	size_t index = 0;
	size_t zero_count = 0;
	while(index < header.width * header.height){
		uint8_t op;
		if(!fread(&op,sizeof(op),1,file))return -1;

		//check for end indicator
		if(!op){
			zero_count++;
		} else if(op == 1 && zero_count >= 7){
			//end
			//the last 7 that we put where corrupted
			index -= 7;
			break;
		} else {
			zero_count = 0;
		}

		switch(op){
		case QOI_OP_RGB:
			if(!fread(&prev.r,sizeof(prev.r),1,file))return -1;
			if(!fread(&prev.g,sizeof(prev.g),1,file))return -1;
			if(!fread(&prev.b,sizeof(prev.b),1,file))return -1;
			texture->bitmap[index++] = gfx_color_rgba(gfx,prev.r,prev.g,prev.b,prev.a);
			pixels_buf[QOI_COLOR_HASH(prev)] = prev;
			continue;
		case QOI_OP_RGBA:
			if(!fread(&prev.r,sizeof(prev.r),1,file))return -1;
			if(!fread(&prev.g,sizeof(prev.g),1,file))return -1;
			if(!fread(&prev.b,sizeof(prev.b),1,file))return -1;
			if(!fread(&prev.a,sizeof(prev.a),1,file))return -1;
			texture->bitmap[index++] = gfx_color_rgba(gfx,prev.r,prev.g,prev.b,prev.a);
			pixels_buf[QOI_COLOR_HASH(prev)] = prev;
			continue;;
		}
		switch((op >> 6) & 0b11){
		case QOI_OP_INDEX:;
			uint8_t i = op & 0b111111;
			prev = pixels_buf[i];
			texture->bitmap[index++] = gfx_color_rgba(gfx,prev.r,prev.g,prev.b,prev.a);
			continue;
		case QOI_OP_DIFF:
			prev.r += ((op >> 4) & 0x3) - 2;
			prev.g += ((op >> 2) & 0x3) - 2;
			prev.b += ((op >> 0) & 0x3) - 2;
			texture->bitmap[index++] = gfx_color_rgba(gfx,prev.r,prev.g,prev.b,prev.a);
			pixels_buf[QOI_COLOR_HASH(prev)] = prev;
			continue;
		case QOI_OP_LUMA:;
			int8_t gd = (op & 0x3f) - 32;
			uint8_t b2;
			if(!fread(&b2,sizeof(b2),1,file))return -1;
			prev.g += gd;
			prev.r += ((b2 >> 4) & 0xf) + gd - 8;
			prev.b += ((b2 >> 0) & 0xf) + gd - 8;
			texture->bitmap[index++] = gfx_color_rgba(gfx,prev.r,prev.g,prev.b,prev.a);
			pixels_buf[QOI_COLOR_HASH(prev)] = prev;
			continue;
		case QOI_OP_RUN:;
			uint8_t count = (op & 0b111111) + 1;
			color_t col = gfx_color_rgba(gfx,prev.r,prev.g,prev.b,prev.a);
			while(count-- > 0){
				texture->bitmap[index++] = col;
			}
			continue;
		}
	}

	color_t col = gfx_color_rgba(gfx,prev.r,prev.g,prev.b,prev.a);
	while(index < header.width * header.height){
		texture->bitmap[index++] = col;
	}

	return 0;
}