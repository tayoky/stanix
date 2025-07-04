#include <kernel/limine.h>
#include <kernel/framebuffer.h>
#include <kernel/kernel.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/devices.h>
#include <kernel/bootinfo.h>
#include <kernel/memseg.h>
#include <sys/mman.h>
#include <sys/fb.h>
#include <errno.h>

ssize_t framebuffer_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	struct limine_framebuffer *inode = node->private_inode;
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

int framebuffer_ioctl(vfs_node *node,uint64_t request,void *arg){
	struct limine_framebuffer *inode = node->private_inode;
	
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

void framebuffer_unmap(memseg *seg){
	(void)seg;
}

void *frambuffer_mmap(vfs_node *node,void *addr,size_t lenght,uint64_t prot,int flags,off_t offset){
	if(!(flags & MAP_SHARED)){
		return (void *)-EINVAL;
	}

	struct limine_framebuffer *inode = node->private_inode;
	offset = PAGE_ALIGN_DOWN(offset);
	lenght = PAGE_ALIGN_DOWN(lenght);
	if(lenght == 0)return (void *)-EINVAL;

	memseg *seg = memseg_create(get_current_proc(),(uintptr_t)addr,lenght,prot,flags);
	if(!seg)return (void *)-EEXIST;
	seg->unmap = framebuffer_unmap;

	uintptr_t vaddr = seg->addr;
	uintptr_t paddr = (uintptr_t)inode->address - kernel->hhdm + offset;
	uintptr_t end   = paddr + lenght;

	kdebugf("map framebuffer at %p in %p lenght : %p\n",vaddr,seg,lenght);

	while(paddr < end){
		map_page(get_current_proc()->addrspace,paddr,vaddr,prot);
		paddr += PAGE_SIZE;
		vaddr += PAGE_SIZE;
	}

	return (void *)seg->addr;
}

void draw_pixel(vfs_node *framebuffer,uint64_t x,uint64_t y,uint32_t color){
	struct limine_framebuffer *inode = framebuffer->private_inode;
	uint64_t location =  y * inode->pitch  + (x * sizeof(uint32_t));
	vfs_write(framebuffer,&color,location,sizeof(uint32_t));
}

void init_frambuffer(void){
	kstatus("init frambuffer ...");

	for (uint64_t i = 0; i < frambuffer_request.response->framebuffer_count; i++){
		if(i >= 100){
			kfail();
			kinfof("too many frambuffer (%ld) can only have up to 99\n",frambuffer_request.response->framebuffer_count);
			kinfof("can still continue init\n");
			return;
		}

		//find the path for the frambuffer
		char *full_path = kmalloc(strlen("/dev/fb") + 3);
		sprintf(full_path,"/dev/fb%d",i);

		vfs_node *framebuffer_dev = kmalloc(sizeof(vfs_node));
		memset(framebuffer_dev,0,sizeof(vfs_node));
		framebuffer_dev->flags = VFS_DEV | VFS_BLOCK;
		framebuffer_dev->write = framebuffer_write;
		framebuffer_dev->mmap  = frambuffer_mmap;
		framebuffer_dev->ioctl = framebuffer_ioctl;
		framebuffer_dev->private_inode = frambuffer_request.response->framebuffers[i];

		//create the device
		if(vfs_mount(full_path,framebuffer_dev)){
			kfail();
			kinfof("fail to create device %s\n",full_path);
			kfree(full_path);
			return;
		}
		kfree(full_path);
	}
	kok();
	
	kinfof("%lu frambuffer found\n",frambuffer_request.response->framebuffer_count);
}