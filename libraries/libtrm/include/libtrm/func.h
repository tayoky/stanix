#ifndef _LIBTRM_FUNC_H
#define _LIBTRM_FUNC_H

#include "trm.h"

trm_card_t *trm_get_ressources(int fd);
trm_fb_t *trm_get_framebuffer(int fd, uint32_t id);
trm_plane_t *trm_get_plane(int fd, uint32_t id);
trm_connector_t *trm_get_connector(int fd, uint32_t id);
void trm_free_framebuffer(trm_fb_t *fb);
trm_fb_t *trm_allocate_framebuffer(int fd, uint32_t width, uint32_t height, uint32_t format);
void *trm_mmap_framebuffer(trm_fb_t *fb);

static inline int trm_test_mode(int fd, trm_mode_t *mode) {
	return ioctl(fd, TRM_KMS_TEST, mode);
}

static inline int trm_fix_mode(int fd, trm_mode_t *mode) {
	return ioctl(fd, TRM_KMS_FIX, mode);
}

static inline int trm_commit_mode(int fd, trm_mode_t *mode) {
	return ioctl(fd, TRM_KMS_COMMIT, mode);
}

#endif
