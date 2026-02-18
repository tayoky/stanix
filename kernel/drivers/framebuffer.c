#include <kernel/limine.h>
#include <kernel/framebuffer.h>
#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/device.h>
#include <kernel/print.h>
#include <kernel/vmm.h>
#include <kernel/tmpfs.h>
#include <sys/fb.h>
#include <errno.h>

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
	framebuffer_t *framebuffer = fd->private;
	
	if (offset + count > framebuffer->size) {
		if ((size_t)offset > framebuffer->size) {
			return 0;
		}
		count = framebuffer->size - offset;
	}
	if (count == sizeof(uint32_t)) {
		// special case if we set only one pixel to go faster
		*(uint32_t *)(((char*)framebuffer->base) + offset + kernel->hhdm) = *(uint32_t *)buffer;
		return sizeof(uint32_t);
	}

	// write to the framebuffer is easy just memcpy
	memcpy((char*)framebuffer->base + offset + kernel->hhdm, buffer, count);

	return count;
}

// really bad scrolling
static int framebuffer_scroll(framebuffer_t *framebuffer, size_t count) {
	uint32_t *buffer = (uint32_t *)framebuffer->base;
	struct fb info;
	int ret = framebuffer_get_info(framebuffer, &info);
	if (ret < 0) return ret;

	memmove(buffer, &buffer[count * info.width], (info.height - count) * info.width * sizeof(uint32_t));
	
	return 0;
}

static int framebuffer_ioctl(vfs_fd_t *fd, long request, void *arg) {
	framebuffer_t *framebuffer = fd->private;
	
	switch (request){
	case IOCTL_GET_FB_INFO:
		return framebuffer_get_info(framebuffer, arg);
	case IOCTL_FRAMEBUFFER_SCROLL:
		return framebuffer_scroll(framebuffer, (size_t)arg);
		break;
	default:
		return -EINVAL;
		break;
	}
}

static int framebuffer_mmap(vfs_fd_t *fd, off_t offset, vmm_seg_t *seg) {
	framebuffer_t *framebuffer = fd->private;
	if(!(seg->flags & VMM_FLAG_SHARED)){
		return -EINVAL;
	}

	// the framebuffer must be mapped as IO
	seg->flags |= VMM_FLAG_IO;

	if (framebuffer->ops->mmap) {
		return framebuffer->ops->mmap(framebuffer, offset, seg);
	}

	kdebugf("map framebuffer at %p in %p lenght : %p\n", seg->start, seg, VMM_SIZE(seg));
	
	uintptr_t paddr = framebuffer->base + PAGE_ALIGN_DOWN(offset);
	for (uintptr_t vaddr=seg->start; vaddr < seg->end; vaddr += PAGE_SIZE) {
		mmu_map_page(get_current_proc()->vmm_space.addrspace, paddr, vaddr, seg->prot);
		paddr += PAGE_SIZE;
	}

	return 0;
}

void draw_pixel(vfs_fd_t *fd, size_t x, size_t y, uint32_t color, struct fb *info) {
	uintptr_t location =  y * info->pitch  + (x * sizeof(uint32_t));
	vfs_write(fd, &color, location, sizeof(uint32_t));
}

static vfs_fd_ops_t framebuffer_ops = {
	.write = framebuffer_write,
	.mmap  = framebuffer_mmap,
	.ioctl = framebuffer_ioctl,
};


int register_framebuffer(framebuffer_t *fb) {
	fb->device.ops = &framebuffer_ops;
	fb->device.type = VFS_BLOCK;
	// TODO : use name allocator
	fb->device.name = strdup("fb0");
	return register_device((device_t*)fb);
}
