#ifndef _GFX_INTERNAL_H
#define _GFX_INTERNAL_H

#include "gfx.h"
#include <stdio.h>

int psf1_load(font_t *font,const char *path);
int bmp_load(gfx_t*,texture_t *texture,FILE *file);
int qoi_load(gfx_t *gfx,texture_t *texture,FILE *file);

#endif