#ifndef KERNEL_FRAMEBUFFER_H
#define KERNEL_FRAMEBUFFER_H

#include <kernel/vfs.h>
#include <kernel/device.h>
#include <kernel/list.h>
#include <kernel/edid.h>
#include <sys/fb.h>
#include <stdarg.h>

typedef struct framebuffer_ops framebuffer_ops_t;
typedef struct framebuffer framebuffer_t;
struct vmm_seg;

struct framebuffer_ops {
	int (*get_info)(framebuffer_t *fb, struct fb *info);
	int (*get_edid)(framebuffer_t *fb, edid_t **edid);
	int (*mmap)(framebuffer_t *fb, off_t offset, struct vmm_seg *seg);
	int (*ioctl)(framebuffer_t *fb, long req, void *arg);
	void (*destroy)(framebuffer_t *fb);
	void (*cleanup)(framebuffer_t *fb);
};

struct framebuffer {
	device_t device;
	list_node_t node;
	framebuffer_ops_t *ops;
	uintptr_t base;
	size_t size;
};

void init_liminefb(void);
int framebuffer_register(framebuffer_t *framebuffer);
int framebuffer_get_info(framebuffer_t *framebuffer, struct fb *info);
int framebuffer_get_edid(framebuffer_t *framebuffer, edid_t **edid);
void framebuffer_destroy_in_mmio_range(uintptr_t start, size_t size);

#endif
