#include <kernel/limine.h>
#include <kernel/framebuffer.h>
#include <kernel/userspace.h>
#include <kernel/process.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/device.h>
#include <kernel/print.h>
#include <kernel/vmm.h>
#include <kernel/mmu.h>
#include <kernel/tmpfs.h>
#include <sys/fb.h>
#include <errno.h>

static list_t framebuffers;
static spinlock_t framebuffers_lock;

int framebuffer_get_info(framebuffer_t *framebuffer, struct fb *info) {
	if (framebuffer->ops->get_info) {
		return framebuffer->ops->get_info(framebuffer, info);
	} else {
		return -ENOTSUP;
	}
}

int framebuffer_get_edid(framebuffer_t *framebuffer, edid_t **edid) {
	if (framebuffer->ops->get_edid) {
		return framebuffer->ops->get_edid(framebuffer, edid);
	} else {
		return -ENOTSUP;
	}
}

static ssize_t framebuffer_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	framebuffer_t *framebuffer = container_of(fd->private, framebuffer_t, device);
	
	if (offset + count > framebuffer->size) {
		if ((size_t)offset > framebuffer->size) {
			return 0;
		}
		count = framebuffer->size - offset;
	}

	// writing to the framebuffer is easy just memcpy
	if (safe_copy_from(mmu_phys2virt(framebuffer->base + offset), buffer, count) < 0) {
		return -EFAULT;
	}

	return count;
}

static int framebuffer_ioctl(vfs_fd_t *fd, long request, void *arg) {
	framebuffer_t *framebuffer = container_of(fd->private, framebuffer_t, device);
	
	switch (request){
	case IOCTL_GET_FB_INFO:;
		// avoid direct copy
		struct fb fb_info;
		int ret = framebuffer_get_info(framebuffer, &fb_info);
		if (ret < 0) return ret;
		return safe_copy_auto_to(arg, &fb_info);
	default:
		return -ENOTTY;
	}
}

static int framebuffer_mmap(vfs_fd_t *fd, off_t offset, vmm_seg_t *seg) {
	framebuffer_t *framebuffer = container_of(fd->private, framebuffer_t, device);
	if(!(seg->flags & VMM_FLAG_SHARED)){
		return -EINVAL;
	}

	// the framebuffer must be mapped as IO
	seg->flags |= VMM_FLAG_IO;

	if (framebuffer->ops->mmap) {
		return framebuffer->ops->mmap(framebuffer, offset, seg);
	}

	// bound checking
	if (offset + VMM_SIZE(seg) > PAGE_ALIGN_UP(framebuffer->size)) {
		return -EINVAL;
	}
		
	kdebugf("map framebuffer at %p in %p length : %p\n", seg->start, seg, VMM_SIZE(seg));
	seg->prot |= MMU_FLAG_WRITE_COMBINE;
	uintptr_t paddr = framebuffer->base + PAGE_ALIGN_DOWN(offset);
	return mmu_map_pages(seg->space->addrspace, seg->start, paddr, VMM_SIZE(seg), seg->prot);
}

static vfs_fd_ops_t framebuffer_ops = {
	.write = framebuffer_write,
	.mmap  = framebuffer_mmap,
	.ioctl = framebuffer_ioctl,
};

static void framebuffer_destroy(device_t *device) {
	framebuffer_t *framebuffer = container_of(device, framebuffer_t, device);
	spinlock_acquire(&framebuffers_lock);
	list_remove(&framebuffers, &framebuffer->node);
	spinlock_release(&framebuffers_lock);
	if (framebuffer->ops && framebuffer->ops->destroy) {
		framebuffer->ops->destroy(framebuffer);
	}
}

static void framebuffer_cleanup(device_t *device) {
	framebuffer_t *framebuffer = container_of(device, framebuffer_t, device);
	if (framebuffer->ops && framebuffer->ops->cleanup) {
		framebuffer->ops->cleanup(framebuffer);
	}
}

int framebuffer_register(framebuffer_t *framebuffer) {
	static int fb_major = 0;
	if (fb_major == 0) {
		fb_major = device_allocate_major();
	}
	framebuffer->device.ops     = &framebuffer_ops;
	framebuffer->device.destroy = framebuffer_destroy;
	framebuffer->device.cleanup = framebuffer_cleanup;
	framebuffer->device.type    = DEVICE_CHAR;
	int ret = device_register(&framebuffer->device, "fb%d", makedev(fb_major, 0));
	if (ret < 0) return ret;
	spinlock_acquire(&framebuffers_lock);
	list_append(&framebuffers, &framebuffer->node);
	spinlock_release(&framebuffers_lock);
	return 0;
}

void framebuffer_destroy_in_mmio_range(uintptr_t start, size_t size) {
	uintptr_t end = start + size;
	spinlock_acquire(&framebuffers_lock);
	int found = 1;
	while (found) {
		found = 0;
		foreach (node, &framebuffers) {
			framebuffer_t *framebuffer = container_of(node, framebuffer_t, node);
			if (framebuffer->base + framebuffer->size > start && framebuffer->base < end) {
				// this framebuffer is in the mmio region
				device_ref(&framebuffer->device);
				spinlock_release(&framebuffers_lock);
				device_destroy(&framebuffer->device);
				device_release(&framebuffer->device);
				spinlock_acquire(&framebuffers_lock);
				found = 1;
				break;
			}
		}
	}
	spinlock_release(&framebuffers_lock);
}
