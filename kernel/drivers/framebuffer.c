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

uint64_t framebuffer_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	struct limine_framebuffer *inode = ((device_inode *)node->private_inode)->private_inode;
	uint64_t size = inode->width * inode->height * (inode->bpp / 8);
	if(offset + count > size){
		if(offset > size){
			return 0;
		}
		count = size - offset;
	}

	//write to the framebuffer is easy just memcpy
	memcpy((void *)inode->address,buffer,count);

	return count;
}
int framebuffer_ioctl(vfs_node *node,uint64_t request,void *arg){
	struct limine_framebuffer *inode = ((device_inode *)node->private_inode)->private_inode;
	
	//implent basic ioctl : width hight ...
	switch (request){
	case IOCTL_FRAMEBUFFER_HEIGHT:
		return inode->height;
		break;
	case IOCTL_FRAMEBUFFER_WIDTH:
		return inode->width;
		break;
	default:
		//invalid
		return -1;
		break;
	}
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
		if(create_dev(full_path,&framebuffer_op,frambuffer_request.response->framebuffers[i])){
			kfail();
			kinfof("fail to create device %s\n",full_path);
			kfree(full_path);
			return;
		}
		kfree(full_path);
	}
	kok();
	
	kinfof("%lu frambuffer find\n",frambuffer_request.response->framebuffer_count);
}