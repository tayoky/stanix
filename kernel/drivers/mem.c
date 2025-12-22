#include <kernel/device.h>
#include <kernel/string.h>
#include <kernel/kheap.h>

// memory devices

#define DEV_MEM 1
#define DEV_NULL 2
#define DEV_ZERO 5
#define DEV_FULL 7

static ssize_t mem_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	device_t *device = fd->private;
	switch (minor(device->number)) {
	case DEV_NULL:
		return 0;
	case DEV_FULL:
	case DEV_ZERO:
		memset(buf, 0, count);
		return count;
	default:
		return -EINVAL;
	}
}

static ssize_t mem_write(vfs_fd_t *fd, const void *buf, off_t offset, size_t count) {
	device_t *device = fd->private;
	switch (minor(device->number)) {
	case DEV_NULL:
	case DEV_ZERO:
		return count;
	case DEV_FULL:
		return -ENOSPC;
	default:
		return -EINVAL;
	}
}

static vfs_ops_t mem_ops = {
	.read  = mem_read,
	.write = mem_write,
};

static device_driver_t mem_driver = {
	.name = "memory devices",
	.major = 1,
};

static int create_mem_dev(int minor, const char *name) {
	device_t *dev = kmalloc(sizeof(device_t));
	memset(dev, 0, sizeof(device_t));
	dev->number = minor;
	dev->name   = strdup(name);
	dev->driver = &mem_driver;
	int ret = register_device(dev);
	if (ret < 0) {
		kfree(dev->name);
		kfree(dev);
	}
	return ret;
}

void init_mem_devices(void) {
	kstatus("init memory devices ... ");
	register_device_driver(&mem_driver);
	create_dev_mem(DEV_NULL, "null");
	create_dev_mem(DEV_ZERO, "zero");
	create_dev_mem(DEV_FULL, "full");
	kok();
}
