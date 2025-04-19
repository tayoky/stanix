#include <kernel/devices.h>
#include <kernel/vfs.h>
#include <kernel/tmpfs.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/asm.h>
#include <kernel/string.h>
#include <kernel/port.h>
#include <kernel/serial.h>

ssize_t zero_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	//to make compiler happy
	(void)node;
	(void)offset;

	memset(buffer,0,count);
	return count;
}

vfs_node zero_dev = {
	.read = zero_read,
	.flags = VFS_DEV | VFS_CHAR | VFS_BLOCK,
};

///dev/port only exist on x86_64
#ifdef x86_64
ssize_t port_read(vfs_node *node,void *buffer,uint64_t port,size_t count){
	//make compiler happy
	(void)node;

	uint8_t *cbuf = buffer;
	while(count > 0){
		*cbuf = in_byte(port);
		cbuf++;
		count--;
	}
	return count;
}
ssize_t port_write(vfs_node *node,void *buffer,uint64_t port,size_t count){
	//make compiler happy
	(void)node;
	
	uint8_t *cbuf = buffer;
	while(count > 0){
		out_byte(port,*cbuf);
		cbuf++;
		count--;
	}
	return count;
}

vfs_node port_dev = {
	.read = port_read,
	.write = port_write
};
#endif
ssize_t write_serial_dev(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	//make compiler happy
	(void)node;
	(void)offset;

	char *str = (char *)buffer;
	for (size_t i = 0; i < count; i++){
		write_serial_char(str[i]);
	}
	
	return count;
}

vfs_node serial_dev = {
	.write = write_serial_dev,
	.flags = VFS_DEV | VFS_CHAR,
};

static ssize_t null_read(vfs_node *node,void *buf,uint64_t offset,size_t count){
	(void)node;
	(void)buf;
	(void)offset;
	return (ssize_t)count;
}
static ssize_t null_write(vfs_node *node,void *buf,uint64_t offset,size_t count){
	(void)node;
	(void)buf;
	(void)offset;
	return (ssize_t)count;
}

vfs_node null_dev = {
	.read = null_read,
	.write = null_write,
	.flags = VFS_DEV | VFS_CHAR,
};

void init_devices(void){
	kstatus("init dev ...");
	if(vfs_mount("/dev",new_tmpfs())){
		kfail();
		kinfof("fail to mount devfs on /dev\n");
		halt();
	}

	//create some simple devices

	// /dev/zero
	if(vfs_mount("/dev/zero",&zero_dev)){
		kfail();
		kinfof("fail to create device /dev/zero\n");
		halt();
	}

	// /dev/port
#ifdef x86_64
	if(vfs_mount("/dev/port",&port_dev)){
		kfail();
		kinfof("fail to create device /dev/port\n");
		halt();
	}
#endif

	// /dev/console
	if(vfs_mount("/dev/console",&serial_dev)){
		kinfof("fail to create device : /dev/console\n");
	}

	///dev/null
	if(vfs_mount("/dev/null",&null_dev)){
		kfail();
		kinfof("fail to create device /dev/null\n");
		halt();
	}

	kok();
}
