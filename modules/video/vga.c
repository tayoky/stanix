#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/port.h>
#include <kernel/trm.h>
#include <module/pci.h>
#include <module/vga.h>
#include <errno.h>

static device_driver_t vga_driver;

static void vga_seq_out(uint8_t index, uint8_t data) {
	out_byte(VGA_SEQ_INDEX, index);
	out_byte(VGA_SEQ_DATA, data);
}

static uint8_t vga_seq_in(uint8_t index) {
	out_byte(VGA_SEQ_INDEX, index);
	return in_byte(VGA_SEQ_DATA);
}

static void vga_crtc_out(uint8_t index, uint8_t data) {
	out_byte(VGA_CRTC_INDEX, index);
	out_byte(VGA_CRTC_DATA, data);
}

static uint8_t vga_crtc_in(uint8_t index) {
	out_byte(VGA_CRTC_INDEX, index);
	return in_byte(VGA_CRTC_DATA);
}

static int vga_test_mode(trm_gpu_t *gpu, trm_mode_t *mode) {
	(void)gpu;
	if (mode->crtcs) {
		trm_crtc_t *crtc = mode->crtcs;
		if (crtc->timings) {
			trm_timings_t *timings = crtc->timings;
			// we need every horizontal timing to be divisible by 8
			if (timings->hdisplay & 7) return -ENOTSUP;
			if (timings->hsync_start & 7) return -ENOTSUP;
			if (timings->hsync_end & 7) return -ENOTSUP;
			if (timings->htotal & 7) return -ENOTSUP;

			// we need some maximum values
			if (timings->hsync_end - timings->hsync_start > 31) return -ENOTSUP;
			if (timings->htotal - timings->hdisplay > 63) return -ENOTSUP;

			// standard VGA only has two clocks
			if (timings->pixel_clock != 25000000 && timings->pixel_clock != 28000000) return -ENOTSUP;
		}
	}
	if (mode->palette) {
		if (mode->palette->colors_count > 256) return -ENOTSUP;
	}
	return 0;
}

static void vga_wait_vsync(void) {
	while (!(in_byte(VGA_INPUT_STATUS1) & 0x80));
	while (in_byte(VGA_INPUT_STATUS1) & 0x80);
}

static int vga_commit_mode(trm_gpu_t *gpu, trm_mode_t *mode) {
	trm_timings_t *timings = NULL;
	if (mode->crtcs) {
		timings = mode->crtcs[0].timings;
	}

	// wait for a retrace
	vga_wait_vsync();

	// disable and reset sequencer
	vga_seq_out(VGA_SEQ_RESET, 0x01);

	// make sure we are not using old monlchrome registers
	uint8_t misc = in_byte(VGA_MISC_READ);
	misc &= ~1;

	// select the right clock
	if (timings) {
		misc &= ~12;
		if (timings->pixel_clock == 25000000) {
			misc |= 0;
		} else if (timings->pixel_clock == 28000000) {
			misc |= 4;
		} else {
			// the test did not catch this ??? WTF ???
			return -ENOTSUP;
		}
	}
	out_byte(VGA_MISC_WRITE, misc);

	// make sure to disable write protect
	uint8_t vsync_end = vga_crtc_in(VGA_CRTC_VSYNC_END);
	vsync_end &= ~0x80;
	vga_crtc_out(VGA_CRTC_VSYNC_END, vsync_end);

	if (timings) {
		// make sure we use 8 pixel char
		uint8_t clocking = vga_seq_in(VGA_SEQ_CLOCK_MODE);
		clocking |= 1;
		vga_seq_out(VGA_SEQ_CLOCK_MODE, clocking);

		// send horizontal timings
		uint8_t hblank_end = timings->htotal / 8;
		vga_crtc_out(VGA_CRTC_HTOTAL, (timings->htotal / 8) - 5);
		vga_crtc_out(VGA_CRTC_HDISPLAY, (timings->hdisplay / 8) - 1);
		vga_crtc_out(VGA_CRTC_HBLANK_START, timings->hdisplay / 8);
		vga_crtc_out(VGA_CRTC_HBLANK_END, 0x80 | (hblank_end & 0x1f));
		vga_crtc_out(VGA_CRTC_HSYNC_START, timings->hsync_start / 8);
		vga_crtc_out(VGA_CRTC_HSYNC_END, ((hblank_end << 2) & 0x80) | ((timings->hsync_end / 8) & 0x1f));

		// send vertical timings
		uint8_t overflows = 0;
		vga_crtc_out(VGA_CRTC_VTOTAL, timings->vtotal);
		overflows |= (timings->vsync_start >> 2) & 0x80;
		overflows |= (timings->vdisplay >> 3) & 0x40;
		overflows |= (timings->vtotal >> 4) & 0x20;
		overflows |= (timings->vdisplay >> 5) & 0x08;
		overflows |= (timings->vsync_start >> 6) & 0x04;
		overflows |= (timings->vdisplay >> 7) & 0x02;
		overflows |= (timings->vtotal >> 8) & 0x01;
		vga_crtc_out(VGA_CRTC_OVERFLOW, overflows);
		vga_crtc_out(VGA_CRTC_MAX_SCAN, (timings->vdisplay >> 4) & 0x20);
		vga_crtc_out(VGA_CRTC_VSYNC_START, timings->vsync_start);
		vga_crtc_out(VGA_CRTC_VSYNC_END, timings->vsync_end & 0x0f);
		vga_crtc_out(VGA_CRTC_VDISPLAY, timings->vdisplay);
		vga_crtc_out(VGA_CRTC_VBLANK_START, timings->vdisplay);
		vga_crtc_out(VGA_CRTC_VBLANK_END, timings->vtotal & 0x7f);
	}

	trm_plane_t *plane = &mode->planes[0];
	if (plane) {
		trm_framebuffer_t *fb = trm_get_fb(gpu, plane->fb_id);
		if (!fb) {
			// TRM didn't catch this ??
			return -EINVAL;
		}
		vga_crtc_out(VGA_CRTC_ADDR_LOW, fb->base);
		vga_crtc_out(VGA_CRTC_ADDR_HIGH, fb->base >> 8);
	}

	if (mode->palette) {
		trm_palette_t *palette = mode->palette;
		for (size_t i=0; i<palette->colors_count; i++) {
			out_byte(VGA_DAC_INDEX_WR, i);
			out_byte(VGA_DAC_DATA, (palette->colors[i] >> 18) & 0x3f);
			out_byte(VGA_DAC_DATA, (palette->colors[i] >> 10) & 0x3f);
			out_byte(VGA_DAC_DATA, (palette->colors[i] >> 2) & 0x3f);
		}
	}

	// restart sequencer
	vga_seq_out(VGA_SEQ_RESET, 0x03);

	return 0;
}

static int vga_support_format(trm_gpu_t *gpu, uint32_t format) {
	(void)gpu;
	// VGA only support indexed colors
	if (format == TRM_C8) return 1;
	return 0;
}

static trm_ops_t vga_ops = {
	.test_mode = vga_test_mode,
	.commit_mode = vga_commit_mode,
	.support_format = vga_support_format,
};

static int vga_check(bus_addr_t *addr) {
    pci_addr_t *pci_addr = (pci_addr_t*)addr;
    if (addr->type != BUS_PCI) return 0;
    if (pci_addr->class == 0x03 && pci_addr->subclass == 0x00) {
		kdebugf("found VGA card\n");
        return 1;
    }
    return 0;
}

static int vga_probe(bus_addr_t *addr) {
    trm_gpu_t *gpu = kmalloc(sizeof(trm_gpu_t));
    memset(gpu, 0, sizeof(trm_gpu_t));
    strcpy(gpu->card.name, "VGA compatible controller");
    gpu->card.vram_size = 256 * 1024;
    gpu->ops = &vga_ops;
    gpu->device.driver = &vga_driver;

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
    addr->device = (device_t*)gpu;
    return 0;
}

static device_driver_t vga_driver = {
    .name = "vga driver",
    .check = vga_check,
    .probe = vga_probe,
};

int vga_init(int argc, char **argv){
	(void)argc;
	(void)argv;
    register_device_driver(&vga_driver);
	return 0;
}

int vga_fini(){
    unregister_device_driver(&vga_driver);
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = vga_init,
	.fini = vga_fini,
	.author = "tayoky",
	.name = "vga driver",
	.description = "TRM vga driver",
	.license = "GPL 3",
};
