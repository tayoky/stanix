#ifndef KERNEL_FBCON_H
#define KERNEL_FBCON_H

#include <kernel/vfs.h>
#include <kernel/device.h>
#include <kernel/tty.h>
#include <kernel/framebuffer.h>
#include <stdint.h>

#define FONT_TYPE_PSF1 0x01

typedef struct fbcon {
	tty_t tty;
	void *header;
	void *font;
	char font_type;
	char activate;
	vfs_fd_t *framebuffer_dev;
	struct fb fb_info;
	uint32_t font_color;
	uint32_t back_color;
	size_t x;
	size_t y;
	size_t width;
	size_t height;
	int ANSI_esc_mode;
} fbcon_t;

typedef struct {
	uint16_t magic; // Magic bytes for identification.
	uint8_t fontMode; // PSF font mode.
	uint8_t characterSize; // PSF character size.
} PSF1_Header;

#define PSF1_FONT_MAGIC 0x0436

void init_fbcon(void);
void fbcon_draw_char(fbcon_t *fbcon, char c);

#endif
