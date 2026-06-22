#include <sys/mman.h>
#include <osmesa.h>
#include <twm.h>

static OSMesaCreateContext_t _OSMesaCreateContext;
static OSMesaCreateContextAttribs_t _OSMesaCreateContextAttribs;
static OSMesaDestroyContext_t _OSMesaDestroyContext;
static OSMesaMakeCurrent_t _OSMesaMakeCurrent;
static OSMesaPixelStore_t _OSMesaPixelStore; 
static OSMesaGetProcAddress_t _OSMesaGetProcAddress;
static void *libOSMesa;

static int twm_load_libOSMesa(void) {
	if (libOSMesa) {
		return 0;
	}
	libOSMesa = dlopen("libOSMesa.so", RTLD_NOW);
	if (!libOSMesa) return -1;

	_OSMesaCreateContext = dlsym(libOSMesa, "OSMesaCreateContext");
	_OSMesaCreateContextAttribs = dlsym(libOSMesa, "OSMesaCreateContextAttribs");
	_OSMesaDestroyContext = dlsym(libOSMesa, "OSMesaDestroyContext");
	_OSMesaMakeCurrent = dlsym(libOSMesa, "OSMesaMakeCurrent");
	_OSMesaPixelStore = dlsym(libOSMesa, "OSMesaPixelStore");
	_OSMesaGetProcAddress = dlsym(libOSMesa, "OSMesaGetProcAddress");
	return 0;
}

twm_opengl_t *twm_get_window_opengl(twm_window_t window) {
	if (twm_load_libOSMesa() < 0) {
		return NULL;
	}

	// get the framebuffer
	int fd;
	twm_fb_info_t fb_info;
	if (twm_get_window_fb(window, &fd, &fb_info) < 0) {
		return NULL;
	}

	// mmap the framebuffer
	size_t fb_size = fb_info.pitch * fb_info.height;
	void *framebuffer = mmap(NULL, fb_size, PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (framebuffer == MAP_FAILED) {
		return NULL;
	}

	OSMesaContext ctx = _OSMesaCreateContext(OSMESA_RGBA, NULL);
	if (!ctx) {
unmap:
		munmap(framebuffer, fb_size);
		return NULL;
	}

	// inverse Y axis
	_OSMesaPixelStore(OSMESA_ROW_LENGTH, fb_info.width);
	_OSMesaPixelStore(OSMESA_Y_UP, 0);

	if (!_OSMesaMakeCurrent(ctx, framebuffer, GL_UNSIGNED_BYTE, fb_info.width, fb_info.height)) {
		_OSMesaDestroyContext(ctx);
		goto unmap;
	}

	twm_opengl_t *opengl = malloc(sizeof(twm_opengl_t));
	opengl->private = ctx;
	opengl->framebuffer = framebuffer;
	opengl->fb_size = fb_size;

	return opengl;
}

void *twm_opengl_get_proc_addr(const char *name) {
	if (twm_load_libOSMesa() < 0) {
		return NULL;
	}
	return _OSMesaGetProcAddress(name);
}

void twm_opengl_destroy(twm_opengl_t *opengl) {
	if (!opengl) return;
	_OSMesaDestroyContext(opengl->private);
	munmap(opengl->framebuffer, opengl->fb_size);
	free(opengl);
}
