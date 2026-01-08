#ifndef _KERNEL_FRAMEBUFFER_H
#define _KERNEL_FRAMEBUFFER_H

#include <kernel/vfs.h>
#include <kernel/limine.h>
#include <kernel/device.h>
#include <kernel/edid.h>
#include <sys/fb.h>
#include <stdarg.h>

typedef struct framebuffer {
	device_t device;
	struct fb info;
	edid_t *edid;
	struct limine_framebuffer *fb;
} framebuffer_t;

void init_frambuffer(void);

void draw_pixel(vfs_fd_t *framebuffer,uint64_t x,uint64_t y,uint32_t color);

#define IOCTL_FRAMEBUFFER_SCROLL 0x0B

#endif