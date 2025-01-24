#include "devices.h"
#include "vfs.h"
#include "tmpfs.h"
#include "kernel.h"
#include "print.h"
#include "asm.h"
#include "string.h"

uint64_t zero_read(vfs_node *node,const void *buffer,uint64_t offset,size_t count){
	memset(buffer,0,count);
	return count;
}

device_op zero_op = {
	.read = zero_read
};

void init_devices(void){
	kstatus("init dev ...");
	if(vfs_mount("dev",new_tmpfs())){
		kfail();
		kinfof("fail to mount devfs on dev:/\n");
		halt();
	}
	if(vfs_create_dev("dev:/zero",&zero_op,NULL)){
		kfail();
		kinfof("fail to create device dev:/zero\n");
		halt();
	}
	kok();
}