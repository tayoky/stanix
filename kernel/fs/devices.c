#include "devices.h"
#include "vfs.h"
#include "tmpfs.h"
#include "kernel.h"
#include "print.h"
#include "asm.h"
#include "string.h"



int create_dev(const char *path,device_op *device,void *private_inode){
	//first create a normal file
	int ret = vfs_create(path,777);
	if(ret){
		return ret;
	}

	//open it
	vfs_node *dev_file = vfs_open(path);
	if(!dev_file){
		return -1;
	}

	//set the dev_inode
	ret = vfs_ioctl(dev_file,IOCTL_TMPFS_SET_DEV_INODE,private_inode);
	if(ret){
		vfs_close(dev_file);
		return ret;
	}
	
	//then turn in into a device
	ret = vfs_ioctl(dev_file,IOCTL_TMPFS_CREATE_DEV,device);
	if(ret){
		vfs_close(dev_file);
		return ret;
	}

	vfs_close(dev_file);
	return 0;
}


void init_devices(void){
	kstatus("init dev ...");
	if(vfs_mount("dev",new_tmpfs())){
		kfail();
		kinfof("fail to mount devfs on dev:/\n");
		halt();
	}
	kok();
}