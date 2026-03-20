#include <sys/fb.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#include <twm.h>
#include <gfx.h>

// libgfx support for twm

static void *libgfx;
static gfx_t *(*_gfx_create)(void *framebuffer, struct fb *fb_info);
void (*_gfx_disable_backbuffer)(gfx_t *gfx);

static int twm_load_libgfx(void) {
	if (!libgfx) {
		libgfx = dlopen("libgfx.so", RTLD_NOW);
		if (!libgfx) return -1;
	}

	if (!_gfx_create) {
		_gfx_create = dlsym(libgfx, "gfx_create");
		if (!_gfx_create) return -1;
	}
	if (!_gfx_disable_backbuffer) {
		_gfx_disable_backbuffer = dlsym(libgfx, "gfx_disable_backbuffer");
		if (!_gfx_disable_backbuffer) return -1;
	}
	
	return 0;
}

gfx_t *twm_get_window_gfx(twm_window_t window) {
	// first make sure libgfx is loaded
	// we cannot direcly link with it
	// because a twm app might not need libgfx
	if (twm_load_libgfx() < 0) return NULL;

	// get the framebuffer
	int fd;
	twm_fb_info_t twm_fb_info;
	if (twm_get_window_fb(window, &fd, &twm_fb_info) < 0) {
		return NULL;
	}

	// then convert the infos into gfx format
	struct fb fb_info = {
		.bpp = twm_fb_info.bpp,
		.red_mask_shift   = twm_fb_info.red_mask_shift,
		.red_mask_size    = twm_fb_info.red_mask_size,
		.green_mask_shift = twm_fb_info.green_mask_shift,
		.green_mask_size  = twm_fb_info.green_mask_size,
		.blue_mask_shift  = twm_fb_info.blue_mask_shift,
		.blue_mask_size   = twm_fb_info.blue_mask_size,
		.width = twm_fb_info.width,
		.height = twm_fb_info.height,
		.pitch = twm_fb_info.pitch,
	};

	// mmap the framebuffer
	void *framebuffrer = mmap(NULL, fb_info.pitch * fb_info.height, PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (framebuffrer == MAP_FAILED) {
		return NULL;
	}

	// and pass everything to libgfx
	gfx_t *gfx = _gfx_create(framebuffrer, &fb_info);
	if (!gfx) return gfx;

	// no need for double buffering the twm server aready do it
	_gfx_disable_backbuffer(gfx);
	return gfx;
}