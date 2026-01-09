#include <kernel/framebuffer.h>
#include <kernel/bootinfo.h>
#include <kernel/kernel.h>
#include <kernel/limine.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/print.h>

// driver for limine's framebuffers

typedef struct limine_fb {
	framebuffer_t framebuffer;
	struct limine_framebuffer *limine_data;
} limine_fb_t;

static int liminefb_get_info(framebuffer_t *fb, struct fb *info) {
	limine_fb_t *liminefb = (limine_fb_t*)fb;

	info->width            = liminefb->limine_data->width;
	info->height           = liminefb->limine_data->height;
	info->pitch            = liminefb->limine_data->pitch;
	info->bpp              = liminefb->limine_data->bpp;
	info->red_mask_size    = liminefb->limine_data->red_mask_size;
	info->red_mask_shift   = liminefb->limine_data->red_mask_shift;
	info->green_mask_size  = liminefb->limine_data->green_mask_size;
	info->green_mask_shift = liminefb->limine_data->green_mask_shift;
	info->blue_mask_size   = liminefb->limine_data->blue_mask_size;
	info->blue_mask_shift  = liminefb->limine_data->blue_mask_shift;

	return 0;
}

static int liminefb_get_edid(framebuffer_t *fb, edid_t **edid) {
	limine_fb_t *liminefb = (limine_fb_t*)fb;

	if (liminefb->limine_data->edid) {
		*edid = liminefb->limine_data->edid;
		return 0;
	} else {
		// limine didn't give a edid
		return -ENOENT;
	}
}

static framebuffer_ops_t liminefb_ops = {
	.get_info = liminefb_get_info,
	.get_edid = liminefb_get_edid,
};

static device_driver_t liminefb_driver = {
	.name = "limineFB driver",
};

void init_liminefb(void) {
	kstatusf("init liminefb ...");
	register_device_driver(&liminefb_driver);

	for (size_t i = 0; i < framebuffer_request.response->framebuffer_count; i++){
		struct limine_framebuffer *limine_data = framebuffer_request.response->framebuffers[i];

		limine_fb_t *framebuffer_dev = kmalloc(sizeof(limine_fb_t));
		memset(framebuffer_dev, 0, sizeof(limine_fb_t));
		
		framebuffer_dev->framebuffer.device.driver = &liminefb_driver;
		framebuffer_dev->framebuffer.ops  = &liminefb_ops;
		framebuffer_dev->framebuffer.base = (uintptr_t)limine_data->address - kernel->hhdm;
		framebuffer_dev->framebuffer.size = limine_data->height * limine_data->pitch;
		framebuffer_dev->limine_data = limine_data;

		// create the device
		if (register_framebuffer((framebuffer_t*)framebuffer_dev)) {
			kfail();
			kinfof("fail to create device /dev/%s\n", framebuffer_dev->framebuffer.device.name);
			return;
		}
	}
	kok();
	
	kinfof("%lu framebuffer found\n", framebuffer_request.response->framebuffer_count);
}
