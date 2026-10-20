#ifndef KERNEL_FRAMEBUFFER_H
#define KERNEL_FRAMEBUFFER_H

#include <kernel/vfs.h>
#include <kernel/device.h>
#include <kernel/edid.h>
#include <sys/fb.h>
#include <stdarg.h>

struct framebuffer;
struct vmm_seg;

typedef struct framebuffer_ops {
	int (*get_info)(struct framebuffer *fb, struct fb *info);
	int (*get_edid)(struct framebuffer *fb, edid_t **edid);
	int (*mmap)(struct framebuffer *fb, off_t offset, struct vmm_seg *seg);
	int (*ioctl)(struct framebuffer *fb, long req, void *arg);
	void (*cleanup)(struct framebuffer *fb);
} framebuffer_ops_t;

typedef struct framebuffer {
	device_t device;
	framebuffer_ops_t *ops;
	uintptr_t base;
	size_t size;
} framebuffer_t;

void init_liminefb(void);
int framebuffer_register(framebuffer_t *fb);
int framebuffer_get_info(framebuffer_t *framebuffer, struct fb *info);
int framebuffer_get_edid(framebuffer_t *framebuffer, edid_t **edid);

#endif
