#include <kernel/trm.h>
#include <kernel/vfs.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <kernel/memseg.h>

#define TRM_NO_ALLOC ((uintptr_t)-1)

static uintptr_t trm_alloc(trm_gpu_t *gpu, size_t size) {
	if (size % gpu->align) {
		size += gpu->align - (size % gpu->align);
	}
	foreach (node, gpu->alloc_blocks) {
		trm_alloc_block_t *block = node->value;
		if (!block->free || block->size < size) {
			continue;
		}

		// we found a correct block
		if (block->size > size) {
			trm_alloc_block_t *new_block = kmalloc(sizeof(trm_alloc_block_t));
			new_block->size = block->size - size;
			new_block->base = block->base + size;
			new_block->free = 1;
			block->size = size;
			list_add_after(gpu->alloc_blocks, node, new_block);
		}
		block->free = 0;
		return block->base;
	}
	return TRM_NO_ALLOC;
}

static void trm_fb_mmap(vfs_fd_t *fd, off_t offset, memseg_t *seg) {
	trm_framebuffer_t *fb = fd->private;
	if (offset + seg->size > fb->fb.pitch * fb->fb.height) {
		return -EINVAL;
	}
	if (!fb->gpu->ops->mmap) {
		return -EINVAL;
	}
	return fb->gpu->ops->mmap(fb->gpu, fb->base + offset, seg);
}

static void trm_fb_close(vfs_fd_t *fd) {
	trm_framebuffer_t *fb = fd->private;
	libutils_hashmap_remove(&fb->gpu-fbs, fb->fb.id);
	// TODO : free used vram
	kfree(fb);
}

static vfs_ops_t trm_fb_ops = {
	.mmap  = trm_fb_mmap,
	.close = trm_fb_close,
};

static int trm_alloc_fb(vfs_fd_t *fd, trm_gpu_t *gpu, trm_fb_t *fb) {
	// TODO : check if the card support the format
	if (!fb->pitch) {
		// TODO : calculate pitch
	}
	size_t size = fb->pitch * fb->height;
	uintptr_t base = trm_alloc(gpu, size);
	if (base == TRM_NO_ALLOC) {
		return -ENOMEM;
	}

	fb->id = gpu->fd_count++;

	trm_framebuffer_t *framebuffer = kmalloc(sizeof(trm_framebuffer_t));
	framebuffer->fb = fb;
	framebuffer->base = base;
	framebuffer->owner = fb;
	framebuffer->gpu = gpu;

	vfs_fd_t *fb_fd = kmalloc(sizeof(vfs_fd_t));
	memset(fb_fd, 0, sizeof(vfs_fd_t));
	framebuffer->fd = fb_fd;
	fb_fd->ops = trm_fb_ops;
	fb_fd->private = framebuffer;
	fb_fd->type = VFS_BLOCK;
	fb_fd->flags = O_WRONLY;

	// TODO : add the fd to the proc's fd
}

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
		if (!trm_get_fb(mode_plane->fb_id)) return -EINVAL;

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
	case TRM_ALLOC_FRAMEBUFFER:
		return trm_alloc_fb(fd, gpu, arg);
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

static void trm_cleanup(device_t *device) {
	trm_gpu_t *gpu = (trm_gpu_t*)device;
	if (gpu->ops->cleanup) {
		gpu->ops->cleanup(gpu);
	}
	// TODO : cleanup everything
	libutils_free_hashmap(&gpu->fbs);
	kfree(gpu->card.planes);
	kfree(gpu->card.crtc);
	kfree(gpu->card.connectors);
	free_list(gpu->alloc_blocks);
	kfree(device->name);
}

static vfs_ops_t trm_ops = {
	.ioctl = trm_ioctl,
	.open  = trm_open,
	.close = trm_close,
};

trm_framebuffer_t *trm_get_fb(trm_gpu_t *gpu, uint32_t id) {
	return libutils_hashmap_get(gpu->fds, id);
}

int register_trm_gpu(trm_gpu_t *gpu) {
	static int video_count = 0;
	char name[32];
	sprintf(name, "video%d", video_count++);
	gpu->device.name = strdup(name);
	gpu->device.ops  = &trm_ops;
	gpu->device.type = DEVICE_CHAR;

	// default alignement
	if (!gpu->align) gpu->align = 4 * 1024;

	for (size_t i=0; i<gpu->card.planes_count) {
		trm_plane_t *plane =& gpu->card.planes[i];
		plane->id = i + 1;
		if (plane->type == TRM_PLANE_PRIMARY) {
			plane->dest_x = 0;
			plane->dest_y = 0;
			if (plane->crtc) {
				trm_crtc_t *crtc = gpu->card.crtcs[plane->crtc - 1];
				plane->dest_w = crtc->timings.hdisplay;
				plane->dest_h = crtc->timings.vdisplay;
			}
		}
	}
	for (size_t i=0; i<gpu->card.crtcs_count) {
		gpu->card.crtcs[i].id = i + 1;
	}
	for (size_t i=0; i<gpu->card.connectors_count) {
		gpu->card.connectors[i].id = i + 1;
	}

	gpu->alloc_blocks = new_list();
	trm_alloc_block_t *main_block = kmalloc(sizeof(trm_alloc_block_t));
	main_block->size = gpu->card.vram_size;
	main_block->base = 0;
	main_block->free = 1;
	list_append(gpu->alloc_blocks, main_block);
	libutils_init_hashmap(&gpu->fbs, 32);

	return register_device((device_t*)gpu);
}
