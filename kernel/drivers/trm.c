#include <kernel/trm.h>
#include <kernel/vfs.h>
#include <kernel/print.h>
#include <kernel/string.h>

static int trm_check_mode(trm_gpu_t *gpu, trm_mode_t *mode) {
	for (size_t i=0; i<mode->planes_count; i++) {
		trm_plane_t *mode_plane = &mode->planes[i];
		if (mode_plane->id > gpu->card.planes_count) return -EINVAL;
		if (mode_plane->crtc > gpu->card.crtcs_count) return -EINVAL;

		trm_plane_t *gpu_plane = &gpu->card.planes[i];
		if (mode_plane->type != gpu_plane->type) return -EINVAL;
		if (mode_plane->possible_crtcs != gpu_plane->possible_crtcs) return -EINVAL;
		if (!(TRM_CRTC_MASK(mode_plane->crtc) & mode_plane->possible_crtcs)) return -EINVAL;

		trm_crtc_t *crtc = &gpu->card.crtcs[mode_plane->crtc - 1];

		if (mode_plane->type == TRM_PLANE_PRIMARY) {
			// primary planes have some restrictions
			if (mode_plane->dest_x != 0 || mode_plane->dest_y != 0) return -EINVAL;
			//if (mode_plane->dest_w != crtc->timings->hdisplay || mode_plane->dest_h != crtc->timings->vdisplay) return -EINVAL;
		}
	}
	// TODO : check more stuff
	return gpu->ops->test_mode(gpu, mode);
}

static int trm_ioctl(vfs_fd_t *fd, long req, void *arg) {
	trm_gpu_t *gpu = fd->private;
	int ret;
	switch (req) {
	case TRM_GET_RESSOURCES:;
		trm_card_t *card = arg;
		card->planes_count     = gpu->card.planes_count;
		card->crtcs_count      = gpu->card.crtcs_count;
		card->connectors_count = gpu->card.connectors_count;
		if (card->planes)     memcpy(card->planes    , gpu->card.planes    , gpu->card.planes_count     * sizeof(trm_plane_t));
		if (card->crtcs)      memcpy(card->crtcs     , gpu->card.crtcs     , gpu->card.crtcs_count      * sizeof(trm_crtc_t));
		if (card->connectors) memcpy(card->connectors, gpu->card.connectors, gpu->card.connectors_count * sizeof(trm_connector_t));
		memcpy(card->name  , gpu->card.name  , sizeof(gpu->card.name));
		memcpy(card->driver, gpu->card.driver, sizeof(gpu->card.name));
		return 0;
	case TRM_GET_PLANE:;
		trm_plane_t *plane = arg;
		if (plane->id > gpu->card.planes_count) return -EINVAL;
		memcpy(plane, &gpu->card.planes[plane->id - 1], sizeof(trm_plane_t));
		return 0;
	case TRM_KMS_TEST:
		return trm_check_mode(gpu, arg);
	case TRM_KMS_COMMIT:
		if (gpu->master != fd) return -EPERM;
		if (!gpu->ops->commit_mode) return -EINVAL;
		trm_mode_t *mode = arg;
		ret = trm_check_mode(gpu, mode);
		if (ret < 0) return ret;
		return gpu->ops->commit_mode(gpu, mode);
	case TRM_KMS_FIX:;
		if (!gpu->ops->fix_mode) return -EINVAL;
		return gpu->ops->fix_mode(gpu, arg);
	default:
		return -ENOTTY;
	}
}

static  int trm_open(vfs_fd_t *fd) {
	trm_gpu_t *gpu = fd->private;
	if (gpu->master == NULL) {
		// the first proc to open get the master
		gpu->master = fd;
	}
	return 0;
}

static void trm_close(vfs_fd_t *fd) {
	trm_gpu_t *gpu = fd->private;
	if (gpu->master == fd) {
		gpu->master = NULL;
	}
}

static vfs_ops_t trm_ops = {
	.ioctl = trm_ioctl,
	.open  = trm_open,
	.close = trm_close,
};

int register_trm_gpu(trm_gpu_t *gpu) {
	static int video_count = 0;
	char name[32];
	sprintf(name, "video%d", video_count++);
	gpu->device.name = strdup(name);
	gpu->device.ops  = &trm_ops;
	gpu->device.type = DEVICE_CHAR;
	return register_device((device_t*)gpu);
}