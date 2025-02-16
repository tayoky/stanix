#include "limine.h"
#include "framebuffer.h"
#include "kernel.h"
#include "string.h"
#include "print.h"
#include "devices.h"
#include "bootinfo.h"

device_op framebuffer_op = {
	.close = NULL,
	.write = framebuffer_write,
	.ioctl = framebuffer_ioctl
};

int64_t framebuffer_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	struct limine_framebuffer *inode = node->dev_inode;
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
	struct limine_framebuffer *inode = node->dev_inode;
	
	//implent basic ioctl : width hight ...
	switch (request){
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
		//invalid
		return -1;
		break;
	}
}

void draw_pixel(vfs_node *framebuffer,uint64_t x,uint64_t y,uint32_t color){
	struct limine_framebuffer *inode = framebuffer->dev_inode;
	uint64_t location =  y * inode->pitch  + (x * sizeof(uint32_t));
	vfs_write(framebuffer,&color,location,sizeof(uint32_t));
}

void init_frambuffer(void){
	kstatus("init frambuffer ...");
	if(vfs_mkdir("dev:/fb",555)){
		kfail();
		kinfof("fail to create dir dev:/fb/\n");
		return;
	}

	for (uint64_t i = 0; i < frambuffer_request.response->framebuffer_count; i++){
		if(i >= 100){
			kfail();
			kinfof("too many frambuffer (%ld) can only have up to 99\n",frambuffer_request.response->framebuffer_count);
			kinfof("can still continue init\n");
			return;
		}

		//find the path for the frambuffer
		char fb_num[3] = {
			'0' + (i / 10),
			'0' + (i % 10),
			'\0'
		};
		char *full_path = kmalloc(strlen("dev:/fb/") + 3);
		strcpy(full_path,"dev:/fb/");
		strcat(full_path,fb_num);

		//create the device
		if(vfs_create_dev(full_path,&framebuffer_op,frambuffer_request.response->framebuffers[i])){
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