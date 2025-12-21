#include <kernel/limine.h>
#include <kernel/framebuffer.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/devfs.h>
#include <kernel/print.h>
#include <kernel/bootinfo.h>
#include <kernel/memseg.h>
#include <kernel/tmpfs.h>
#include <sys/mman.h>
#include <sys/fb.h>
#include <errno.h>

ssize_t framebuffer_write(vfs_fd_t *fd,void *buffer,uint64_t offset,size_t count){
	struct limine_framebuffer *inode = fd->private;
	uint64_t size = inode->width * inode->height * (inode->bpp / 8);
	if(offset + count > size){
		if(offset > size){
			return 0;
		}
		count = size - offset;
	}
	if(count == sizeof(uint32_t)){
		//special case if we set only one pixel to go faster
		*(uint32_t *)(((uint64_t)inode->address) + offset) = *(uint32_t *)buffer;
		return sizeof(uint32_t);	
	}

	//write to the framebuffer is easy just memcpy
	memcpy((void *)((uint64_t)inode->address) + offset,buffer,count);

	return count;
}

int framebuffer_scroll(struct limine_framebuffer *inode,uint64_t count){
	//scroll the specified amount of pixel
	uint32_t *buffer = (uint32_t *)inode->address;

	memmove(buffer,&buffer[count * inode->width],(inode->width * inode->height - count * inode->width) * sizeof(uint32_t));
	
	return 0;
}

int framebuffer_ioctl(vfs_fd_t *node,long request,void *arg){
	struct limine_framebuffer *inode = node->private;
	
	//implent basic ioctl : width hight ...
	switch (request){
	case IOCTL_GET_FB_INFO:
		struct fb *fb = arg;
		fb->width = inode->width;
		fb->height = inode->height;
		fb->pitch = inode->pitch;
		fb->bpp = inode->bpp;
		fb->red_mask_size    = inode->red_mask_size;
		fb->red_mask_shift   = inode->red_mask_shift;
		fb->green_mask_size  = inode->green_mask_size;
		fb->green_mask_shift = inode->green_mask_shift;
		fb->blue_mask_size   = inode->blue_mask_size;
		fb->blue_mask_shift  = inode->blue_mask_shift;
		return 0;
	case IOCTL_FRAMEBUFFER_HEIGHT:
		return inode->height;
		break;
	case IOCTL_FRAMEBUFFER_WIDTH:
		return inode->width;
		break;
	case IOCTL_FRAMEBUFFER_BPP:
		return inode->bpp;
		break;
	case IOCTL_FRAMEBUFFER_RM:
		return inode->red_mask_size;
		break;
	case IOCTL_FRAMEBUFFER_RS:
		return inode->red_mask_shift;
		break;
	case IOCTL_FRAMEBUFFER_GM:
		return inode->green_mask_size;
		break;
	case IOCTL_FRAMEBUFFER_GS:
		return inode->green_mask_shift;
		break;
	case IOCTL_FRAMEBUFFER_BM:
		return inode->blue_mask_size;
		break;
	case IOCTL_FRAMEBUFFER_BS:
		return inode->blue_mask_shift;
		break;
	case IOCTL_FRAMEBUFFER_SCROLL:
		return framebuffer_scroll(inode,(uint64_t) arg);
		break;
	default:
		return -EINVAL;
		break;
	}
}

void framebuffer_unmap(memseg_t *seg){
	(void)seg;
}

int frambuffer_mmap(vfs_fd_t *fd,off_t offset,memseg_t *seg){
	if(!(seg->flags & MAP_SHARED)){
		return -EINVAL;
	}

	struct limine_framebuffer *fb = fd->private;

	seg->unmap = framebuffer_unmap;

	uintptr_t vaddr = seg->addr;
	uintptr_t paddr = (uintptr_t)fb->address - kernel->hhdm + PAGE_ALIGN_DOWN(offset);
	uintptr_t end   = paddr + seg->size;

	kdebugf("map framebuffer at %p in %p lenght : %p\n",vaddr,seg,seg->size);

	while(paddr < end){
		map_page(get_current_proc()->addrspace,paddr,vaddr,seg->prot);
		paddr += PAGE_SIZE;
		vaddr += PAGE_SIZE;
	}

	return 0;
}

void draw_pixel(vfs_fd_t *fd,uint64_t x,uint64_t y,uint32_t color){
	struct limine_framebuffer *inode = fd->private;
	uint64_t location =  y * inode->pitch  + (x * sizeof(uint32_t));
	vfs_write(fd,&color,location,sizeof(uint32_t));
}

vfs_ops_t framebuffer_ops = {
	.write = framebuffer_write,
	.mmap  = frambuffer_mmap,
	.ioctl = framebuffer_ioctl,
};

void init_frambuffer(void){
	kstatusf("init frambuffer ...");

	for (long i = 0; i < frambuffer_request.response->framebuffer_count; i++){
		if(i >= 100){
			kfail();
			kinfof("too many frambuffer (%ld) can only have up to 99\n",frambuffer_request.response->framebuffer_count);
			kinfof("can still continue init\n");
			return;
		}

		//find the path for the frambuffer
		char *full_path = kmalloc(strlen("fb") + 3);
		sprintf(full_path,"fb%d",i);

		framebuffer_t *framebuffer_dev = kmalloc(sizeof(framebuffer_t));
		memset(framebuffer_dev,0,sizeof(framebuffer_t));
		framebuffer_dev->device.type = DEVICE_BLOCK;
		framebuffer_dev->device.ops = &framebuffer_ops;
		framebuffer_dev->device.name = full_path;
		framebuffer_dev->fb = frambuffer_request.response->framebuffers[i];

		//create the device
		if(register_device(framebuffer_dev)){
			kfail();
			kinfof("fail to create device /dev/%s\n",full_path);
			kfree(full_path);
			return;
		}
	}
	kok();
	
	kinfof("%lu framebuffer found\n",frambuffer_request.response->framebuffer_count);
}