#ifndef _LIBTRM_FORMAT_H
#define _LIBTRM_FORMAT_H

#include <stdint.h>

// indexed color
#define TRM_C1 1
#define TRM_C2 2
#define TRM_C4 3
#define TRM_C8 4

// single color
#define TRM_R1 5
#define TRM_R2 6
#define TRM_R4 7
#define TRM_R8 8

// 8 bit bpp
#define TRM_RGB332 9
#define TRM_BGR223 10

// 16 bit bpp
#define TRM_XRGB4444 11
#define TRM_XBGR4444 12
#define TRM_RGBX4444 13
#define TRM_BGRX4444 14

#define TRM_ARGB4444 15
#define TRM_ABGR4444 16
#define TRM_RGBA4444 17
#define TRM_BGRA4444 18

#define TRM_XRGB1555 19
#define TRM_XBGR1555 20
#define TRM_RGBX5551 21
#define TRM_BGRX5551 22

#define TRM_ARGB1555 23
#define TRM_ABGR1555 24
#define TRM_RGBA5551 25
#define TRM_BGRA5551 26

#define TRM_RGB565 27
#define TRM_BGR565 28

// 24 bits bpp
#define TRM_RGB888 29
#define TEL_BGR888 30

// 32 bits bpp
#define TRM_XRGB8888 31
#define TRM_XBGR8888 32
#define TRM_RGBX8888 33
#define TRM_BGRX8888 34

#define TRM_ARGB8888 35
#define TRM_ABGR8888 36
#define TRM_RGBA8888 37
#define TRM_BGRA8888 38

static inline int trm_bpp(uint32_t format) {
	if (format <= 8) {
		return 1 << ((format - 1) % 4);
	} else if (format <= 10) {
		return 8;
	} else if (format <= 28) {
		return 16;
	} else if (format <= 30) {
		return 24;
	} else if (format <= 38) {
		return 32;
	} else {
		return 1;
	}


#endif
