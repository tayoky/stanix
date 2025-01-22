#include "devices.h"
#include "vfs.h"
#include "tmpfs.h"
#include "kernel.h"
#include "print.h"
#include "asm.h"
#include "string.h"

void dev_close(vfs_node *node){
	device_inode *inode = (device_inode *)node->private_inode;
	if(inode->device->close){
		inode->device->close(node);
	}
	kfree(inode);
}

vfs_node *dev_finddir(vfs_node *node,const char *name){
	vfs_node *ret = tmpfs_finddir(node,name);
	if(!ret)return ret;
	if(!(ret->flags & VFS_FILE)){
		//don't forget to keep the dev finddit instead of the tmpfs one
		ret->finddir = dev_finddir;
		return ret;
	}

	//the request element if a file

	//create our own context
	//the pointer to the device ops is inside the file
	device_op *device;
	vfs_read(ret,&device,0,sizeof(device_op *));
	void *private_inode;
	vfs_read(ret,&private_inode,sizeof(device_op *),sizeof(void *));
	vfs_close(ret);

	//make the inode
	device_inode *inode = kmalloc(sizeof(device_inode));
	inode->device = device;
	inode->private_inode = private_inode;

	//then the node
	ret = kmalloc(sizeof(vfs_node));
	ret->flags = VFS_FILE;

	ret->owner = 0;
	ret->group_owner =0;
	
	ret->private_inode = (void *)inode;

	//copy the file op into the node
	ret->read     = device->read;
	ret->write    = device->write;
	ret->mkdir    = device->mkdir;
	ret->create   = device->create;
	ret->truncate = device->truncate;
	ret->unlink   = device->unlink;
	ret->ioctl    = device->ioctl;
	ret->close    = dev_close;

	return ret;
}

int create_dev(const char *path,device_op *device,void *private_inode){
	//first create a normal file
	int ret = vfs_create(path,777);
	if(ret){
		return ret;
	}
	kdebugf("still here\n");
	//then write the device_op pointer to it
	vfs_node *dev_file = vfs_open(path);
	if(!dev_file){
		return -1;
	}

	if(vfs_write(dev_file,&device,0,sizeof(device_op *)) != sizeof(device_op *)){
		return -1;
	}

	//and the private inode pointer
	if(vfs_write(dev_file,private_inode,sizeof(device_op *),sizeof(void *)) != sizeof(void *)){
		return -1;
	}

	vfs_close(dev_file);
	return 0;
}

vfs_node *new_devfs(void){
	//little trick : it is a tmpfs but with dev_finddir instead of tmpfs_finddir
	vfs_node *root = new_tmpfs();
	if(!root)return NULL;
	root->finddir = dev_finddir;
	return root;
}

void init_devices(void){
	kstatus("init dev ...");
	if(vfs_mount("dev",new_devfs())){
		kfail();
		kinfof("fail to mount devfs on dev:/\n");
		halt();
	}
	kok();
}