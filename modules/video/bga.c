#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/port.h>
#include <kernel/trm.h>
#include <module/pci.h>
#include <module/bga.h>
#include <errno.h>

// TRM's BGA driver

typedef struct bga {
	trm_gpu_t gpu;
	uint16_t version;
	uint16_t max_x;
	uint16_t max_y;
} bga_t;

static device_driver_t bga_driver;

static void bga_write(uint16_t index, uint16_t data) {
	out_word(VBE_DISPI_IOPORT_INDEX, index);
	out_word(VBE_DISPI_IOPORT_DATA, data);
}

static uint16_t bga_read(uint16_t index) {
	out_word(VBE_DISPI_IOPORT_INDEX, index);
	return in_word(VBE_DISPI_IOPORT_DATA);
}

static int bga_test_mode(trm_gpu_t *gpu, trm_mode_t *mode) {
	bga_t *bga = (bga_t*)gpu;
	if (mode->planes_count) {
		trm_plane_t *plane = &mode->planes[0];
		if (plane->src_w != plane->dest_w) return -ENOTSUP;
		if (plane->src_h != plane->dest_h) return -ENOTSUP;
	}
	if (mode->crtcs_count) {
		trm_timings_t *timings = mode->crtcs[0].timings;
		if (timings->hdisplay > bga->max_x) return -ENOTSUP;
		if (timings->vdisplay > bga->max_y) return -ENOTSUP;
	}
	return 0;
}

static int bga_commit_mode(trm_gpu_t *gpu, trm_mode_t *mode) {
	kdebugf("bga commit mode\n");
	bga_t *bga = (bga_t*)gpu;

	bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

	if (mode->crtcs) {
		trm_timings_t *timings = timings = mode->crtcs[0].timings;
		if (timings) {
			bga_write(VBE_DISPI_INDEX_XRES, timings->hdisplay);

			bga_write(VBE_DISPI_INDEX_YRES, timings->vdisplay);
		}
	}

	if (mode->planes) {
		trm_framebuffer_t *fb = trm_get_fb(gpu, mode->planes[0].fb_id);
		uint16_t bpp;
		switch (fb->fb.format) {
		case TRM_C8:
			bpp = VBE_DISPI_BPP_8;
			break;
		case TRM_XRGB1555:
			bpp = VBE_DISPI_BPP_15;
			break;
		case TRM_RGB565:
			bpp = VBE_DISPI_BPP_16;
			break;
		case TRM_RGB888:
			bpp = VBE_DISPI_BPP_24;
			break;
		case TRM_XRGB8888:
			bpp = VBE_DISPI_BPP_32;
			break;
		}
		bga_write(VBE_DISPI_INDEX_BPP, bpp);
		if (bga->version >= VBE_DISPI_ID1) {
			// FIXME : not sure this is correct
			bga_write(VBE_DISPI_INDEX_VIRT_WIDTH, fb->fb.width);
			bga_write(VBE_DISPI_INDEX_VIRT_HEIGHT, gpu->card.vram_size / fb->fb.pitch);
			bga_write(VBE_DISPI_INDEX_X_OFFSET, ((fb->base / trm_bpp(fb->fb.format)) % fb->fb.width) + mode->planes->src_x);
			bga_write(VBE_DISPI_INDEX_Y_OFFSET, fb->base / fb->fb.pitch + mode->planes->src_y);
		}
	}

	uint16_t enable = VBE_DISPI_ENABLED;
	if (bga->version >= VBE_DISPI_ID2) {
		enable |= VBE_DISPI_LFB_ENABLED | VBE_DISPI_NOCLEARMEM;
	}
	bga_write(VBE_DISPI_INDEX_ENABLE, enable);

	return 0;
}

static int bga_support_format(trm_gpu_t *gpu, uint32_t format) {
	bga_t *bga = (bga_t*)gpu;

	// BGA support various format
	switch (format) {
	case TRM_C8:
		return 1;
	case TRM_XRGB1555:
	case TRM_RGB565:
	case TRM_RGB888:
	case TRM_XRGB8888:
		return bga->version >= VBE_DISPI_ID2;
	default:
		return 0;
	}
}

static trm_ops_t bga_ops = {
	.test_mode = bga_test_mode,
	.commit_mode = bga_commit_mode,
	.support_format = bga_support_format,
};

static int bga_check(bus_addr_t *addr) {
	pci_addr_t *pci_addr = (pci_addr_t*)addr;
	if (addr->type != BUS_PCI) return 0;
	if (pci_addr->class == 0x03 && pci_addr->subclass == 0x00 && pci_addr->vendor_id == 0x1234 && pci_addr->device_id == 0x1111) {
		kdebugf("found BGA card\n");
		return 1;
	}
	return 0;
}

static int bga_probe(bus_addr_t *addr) {
	pci_addr_t *pci_addr = (pci_addr_t*)addr;

	// init bga specific stuff
	bga_t *bga = kmalloc(sizeof(bga_t));
	memset(bga, 0, sizeof(bga_t));
	bga->version = bga_read(VBE_DISPI_INDEX_ID);
	if (bga->version >= VBE_DISPI_ID3) {
		uint16_t data = bga_read(VBE_DISPI_INDEX_ENABLE);
		bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_GETCAPS | VBE_DISPI_NOCLEARMEM);
		bga->max_x = bga_read(VBE_DISPI_INDEX_XRES);
		bga->max_y = bga_read(VBE_DISPI_INDEX_YRES);
		bga_write(VBE_DISPI_INDEX_ENABLE, data | VBE_DISPI_NOCLEARMEM);
	} else {
		bga->max_x = VBE_DISPI_MAX_XRES;
		bga->max_y = VBE_DISPI_MAX_YRES;
	}

	trm_gpu_t *gpu = (trm_gpu_t*)bga;
	strcpy(gpu->card.name, "BGA adaptor");
	if (bga->version >= VBE_DISPI_ID2) {
		gpu->card.vram_size = bga_read(VBE_DISPI_INDEX_VIDEO_MEMORY_64K) * 64 * 1024;
	} else {
		gpu->card.vram_size = 8 * 1024 * 1024;
	}
	gpu->vram_mmio = pci_get_bar(pci_addr, 0, 0);
	if (gpu->vram_mmio == PCI_INVALID_BAR) {
		kstatusf("BGA card has an invalid BAR\n");
		kfree(gpu);
		return -EIO;
	}
	gpu->ops = &bga_ops;
	gpu->device.driver = &bga_driver;
	gpu->device.addr = addr;

	// setup one primary plane
	gpu->card.planes_count = 1;
	gpu->card.planes = kmalloc(sizeof(trm_plane_t));
	gpu->card.planes[0].type           = TRM_PLANE_PRIMARY;
	gpu->card.planes[0].possible_crtcs = TRM_CRTC_MASK(1);
	gpu->card.planes[0].crtc           = 1;
	gpu->card.planes[0].src_x          = 0;
	gpu->card.planes[0].src_y          = 0;
	gpu->card.planes[0].src_w          = 0;
	gpu->card.planes[0].src_h          = 0;
    
	// setup one crtc
	gpu->card.crtcs_count = 1;
	gpu->card.crtcs = kmalloc(sizeof(trm_crtc_t));
	gpu->card.crtcs[0].active = 1;
	gpu->card.crtcs[0].timings = kmalloc(sizeof(trm_timings_t));

	// setup one connector
	gpu->card.connectors_count = 1;
	gpu->card.connectors = kmalloc(sizeof(trm_connector_t));
	gpu->card.connectors[0].possible_crtcs = TRM_CRTC_MASK(1);
	gpu->card.connectors[0].crtc           = 1;

	register_trm_gpu(gpu);

	return 0;
}

static device_driver_t bga_driver = {
	.name = "BGA driver",
	.check = bga_check,
	.probe = bga_probe,
	.priority = 2, // use this over VGA driver
};

int bga_init(int argc, char **argv){
	(void)argc;
	(void)argv;
	register_device_driver(&bga_driver);
	return 0;
}

int bga_fini(){
	unregister_device_driver(&bga_driver);
	return 0;
}

kmodule_t module_meta = {
	.magic = MODULE_MAGIC,
	.init = bga_init,
	.fini = bga_fini,
	.author = "tayoky",
	.name = "BGA driver",
	.description = "TRM BGA driver",
	.license = "GPL 3",
};
