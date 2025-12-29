#include <kernel/limine.h>
#include <kernel/framebuffer.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/device.h>
#include <kernel/print.h>
#include <kernel/bootinfo.h>
#include <kernel/memseg.h>
#include <kernel/tmpfs.h>
#include <sys/mman.h>
#include <sys/fb.h>
#include <errno.h>

static ssize_t framebuffer_write(vfs_fd_t *fd,const void *buffer,off_t offset,size_t count){
	framebuffer_t *framebuffer = fd->private;
	struct limine_framebuffer *data = framebuffer->fb;
	uint64_t size = data->width * data->height * (data->bpp / 8);
	if(offset + count > size){
		if((size_t)offset > size){
			return 0;
		}
		count = size - offset;
	}
	if(count == sizeof(uint32_t)){
		//special case if we set only one pixel to go faster
		*(uint32_t *)(((uint64_t)data->address) + offset) = *(uint32_t *)buffer;
		return sizeof(uint32_t);	
	}

	//write to the framebuffer is easy just memcpy
	memcpy((void *)((uint64_t)data->address) + offset,buffer,count);

	return count;
}

static int framebuffer_scroll(struct limine_framebuffer *data,uint64_t count){
	//scroll the specified amount of pixel
	uint32_t *buffer = (uint32_t *)data->address;

	memmove(buffer,&buffer[count * data->width],(data->width * data->height - count * data->width) * sizeof(uint32_t));
	
	return 0;
}

static int framebuffer_ioctl(vfs_fd_t *fd,long request,void *arg){
	framebuffer_t *framebuffer = fd->private;
	struct limine_framebuffer *data = framebuffer->fb;
	
	//implent basic ioctl : width hight ...
	switch (request){
	case IOCTL_GET_FB_INFO:
		struct fb *fb = arg;
		fb->width = data->width;
		fb->height = data->height;
		fb->pitch = data->pitch;
		fb->bpp = data->bpp;
		fb->red_mask_size    = data->red_mask_size;
		fb->red_mask_shift   = data->red_mask_shift;
		fb->green_mask_size  = data->green_mask_size;
		fb->green_mask_shift = data->green_mask_shift;
		fb->blue_mask_size   = data->blue_mask_size;
		fb->blue_mask_shift  = data->blue_mask_shift;
		return 0;
	case IOCTL_FRAMEBUFFER_HEIGHT:
		return data->height;
		break;
	case IOCTL_FRAMEBUFFER_WIDTH:
		return data->width;
		break;
	case IOCTL_FRAMEBUFFER_SCROLL:
		return framebuffer_scroll(data,(uint64_t) arg);
		break;
	default:
		return -EINVAL;
		break;
	}
}

static int frambuffer_mmap(vfs_fd_t *fd,off_t offset,memseg_t *seg){
	if(!(seg->flags & MAP_SHARED)){
		return -EINVAL;
	}

	framebuffer_t *framebuffer = fd->private;
	struct limine_framebuffer *fb = framebuffer->fb;

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
	framebuffer_t *framebuffer = fd->private;
	struct limine_framebuffer *data = framebuffer->fb;
	uint64_t location =  y * data->pitch  + (x * sizeof(uint32_t));
	vfs_write(fd,&color,location,sizeof(uint32_t));
}

static device_driver_t framebuffer_driver = {
	.name = "framebuffer driver",
};

static vfs_ops_t framebuffer_ops = {
	.write = framebuffer_write,
	.mmap  = frambuffer_mmap,
	.ioctl = framebuffer_ioctl,
};

void init_frambuffer(void){
	kstatusf("init frambuffer ...");
	register_device_driver(&framebuffer_driver);

	for (size_t i = 0; i < frambuffer_request.response->framebuffer_count; i++){
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
		framebuffer_dev->device.type   = DEVICE_BLOCK;
		framebuffer_dev->device.ops    = &framebuffer_ops;
		framebuffer_dev->device.name   = full_path;
		framebuffer_dev->device.driver = &framebuffer_driver;
		framebuffer_dev->fb = frambuffer_request.response->framebuffers[i];

		//create the device
		if(register_device((device_t*)framebuffer_dev)){
			kfail();
			kinfof("fail to create device /dev/%s\n",full_path);
			kfree(full_path);
			return;
		}
	}
	kok();
	
	kinfof("%lu framebuffer found\n",frambuffer_request.response->framebuffer_count);
}
