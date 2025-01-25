#include "devices.h"
#include "vfs.h"
#include "tmpfs.h"
#include "kernel.h"
#include "print.h"
#include "asm.h"
#include "string.h"
#include "port.h"

uint64_t zero_read(vfs_node *node,const void *buffer,uint64_t offset,size_t count){
	memset(buffer,0,count);
	return count;
}

device_op zero_op = {
	.read = zero_read
};

uint64_t port_read(vfs_node *node,void *buffer,uint64_t port,size_t count){
	uint8_t *cbuf = buffer;
	while(count > 0){
		*cbuf = in_byte(port);
		cbuf++;
		count--;
	}
	return count;
}

uint64_t port_write(vfs_node *node,void *buffer,uint64_t port,size_t count){
	uint8_t *cbuf = buffer;
	while(count > 0){
		out_byte(port,*cbuf);
		cbuf++;
		count--;
	}
	return count;
}

device_op port_op = {
	.read = port_read,
	.write = port_write
};

void init_devices(void){
	kstatus("init dev ...");
	if(vfs_mount("dev",new_tmpfs())){
		kfail();
		kinfof("fail to mount devfs on dev:/\n");
		halt();
	}

	//create some simple devices

	// dev:/zero
	if(vfs_create_dev("dev:/zero",&zero_op,NULL)){
		kfail();
		kinfof("fail to create device dev:/zero\n");
		halt();
	}

	// dev:/port
	if(vfs_create("dev:/port",&port_op,NULL)){
		kfail();
		kinfof("fail to create device dev:/port\n");
		halt();
	}
	kok();
}