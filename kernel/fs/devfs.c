#include <kernel/devfs.h>
#include <kernel/vfs.h>
#include <kernel/tmpfs.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/asm.h>
#include <kernel/string.h>
#include <kernel/port.h>
#include <kernel/serial.h>

static vfs_node *devfs_root;

int devfs_create_dev(const char *path, vfs_node *dev) {
	return vfs_createat_ext(devfs_root, path, 0666, dev->flags, dev);
}


void devfs_remove_dev(const char *path) {
	vfs_unmountat(devfs_root, path);
}

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
	.flags = VFS_DEV | VFS_CHAR | VFS_TTY,
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
	devfs_root = new_tmpfs();
	if(vfs_mount("/dev",devfs_root)){
		kfail();
		kinfof("fail to mount devfs on /dev\n");
		halt();
	}

	//create some simple devices

	// /dev/zero
	if(devfs_create_dev("zero",&zero_dev)){
		kfail();
		kinfof("fail to create device /dev/zero\n");
		halt();
	}

	// /dev/console
	if(devfs_create_dev("console",&serial_dev)){
		kinfof("fail to create device : /dev/console\n");
	}

	///dev/null
	if(devfs_create_dev("null",&null_dev)){
		kfail();
		kinfof("fail to create device /dev/null\n");
		halt();
	}

	if(vfs_mkdir("/dev/pts",0755)){
		kfail();
		kinfof("fail to mkdir /dev/pts\n");
		halt();
	}

	kok();
}
