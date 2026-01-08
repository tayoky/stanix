#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <trm.h>

trm_card_t *trm_get_resources(int fd) {
	trm_card_t *card = malloc(sizeof(trm_card_t));
	if (!card) goto error;
	memset(card, 0, sizeof(trm_card_t));
	if (ioctl(fd, TRM_GET_RESOURCES, card) < 0) goto error_free;
	card->planes     = malloc(sizeof(trm_plane_t) * card->planes_count);
	card->crtcs      = calloc(sizeof(trm_crtc_t), card->crtcs_count);
	card->connectors = malloc(sizeof(trm_connector_t) * card->connectors_count);
	if (ioctl(fd, TRM_GET_RESOURCES, card) < 0) goto error_free;

	return card;

error_free:
	free(card->planes);
	free(card->crtcs);
	free(card->connectors);
	free(card);
error:
	return NULL;

}

trm_fb_t *trm_get_framebuffer(int fd, uint32_t id) {
	trm_fb_t *fb = malloc(sizeof(trm_fb_t));
	if (!fb) return NULL;
	memset(fb, 0, sizeof(trm_fb_t));
	fb->id = id;
	if (ioctl(fd, TRM_GET_FRAMEBUFFER, fb) < 0) {
		free(fb);
		return NULL;
	}
	return fb;
}

trm_plane_t *trm_get_plane(int fd, uint32_t id) {
	trm_plane_t *plane = malloc(sizeof(trm_plane_t));
	if (!plane) return NULL;
	memset(plane, 0, sizeof(trm_plane_t));
	plane->id = id;
	if (ioctl(fd, TRM_GET_PLANE, plane) < 0) {
		free(plane);
		return NULL;
	}
	return plane;
}

trm_connector_t *trm_get_connector(int fd, uint32_t id) {
	trm_connector_t *connector = malloc(sizeof(trm_connector_t));
	if (!connector) return NULL;
	memset(connector, 0, sizeof(trm_connector_t));
	connector->id = id;
	if (ioctl(fd, TRM_GET_CONNECTOR, connector) < 0) {
		free(connector);
		return NULL;
	}
	return connector;
}

trm_fb_t *trm_allocate_framebuffer(int fd, uint32_t width, uint32_t height, uint32_t format) {
	trm_fb_t *fb = malloc(sizeof(trm_fb_t));
	if (!fb) return NULL;
	memset(fb, 0, sizeof(trm_fb_t));
	fb->width  = width;
	fb->height = height;
	fb->format = format;
	if (ioctl(fd, TRM_ALLOC_FRAMEBUFFER, fb) < 0) {
		free(fb);
		return NULL;
	}

	return fb;
}

void *trm_mmap_framebuffer(trm_fb_t *fb) {
	return mmap(NULL, fb->pitch * fb->height, PROT_WRITE, MAP_SHARED, fb->fd, 0);
}

void trm_free_framebuffer(trm_fb_t *fb) {
	close(fb->fd);
	free(fb);
}
