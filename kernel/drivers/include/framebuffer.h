#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H
#include <kernel/vfs.h>
#include <kernel/limine.h>
#include <kernel/device.h>
#include <stdarg.h>

typedef struct framebuffer {
	device_t device;
	struct limine_framebuffer *fb;
} framebuffer_t;

void init_frambuffer(void);

void draw_pixel(vfs_fd_t *framebuffer,uint64_t x,uint64_t y,uint32_t color);

#define IOCTL_FRAMEBUFFER_WIDTH  0x01
#define IOCTL_FRAMEBUFFER_HEIGHT 0x02
#define IOCTL_FRAMEBUFFER_BPP    0x03
#define IOCTL_FRAMEBUFFER_RM     0x04
#define IOCTL_FRAMEBUFFER_RS     0x05
#define IOCTL_FRAMEBUFFER_GM     0x06
#define IOCTL_FRAMEBUFFER_GS     0x07
#define IOCTL_FRAMEBUFFER_BM     0x08
#define IOCTL_FRAMEBUFFER_BS     0x09
#define IOCTL_FRAMEBUFFER_DRAW_PIXEL 0x0A
#define IOCTL_FRAMEBUFFER_SCROLL 0x0B
#endif