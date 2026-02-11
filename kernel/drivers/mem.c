#include <kernel/device.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/serial.h>

// memory devices

#define DEV_MEM     1
#define DEV_NULL    2
#define DEV_ZERO    5
#define DEV_FULL    7
#define DEV_KMSG    11
#define DEV_TTYBOOT 13

static ssize_t mem_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	(void)offset;
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
	(void)offset;
	device_t *device = fd->private;
	switch (minor(device->number)) {
	case DEV_NULL:
	case DEV_ZERO:
		return count;
	case DEV_FULL:
		return -ENOSPC;
	case DEV_KMSG:
		kprint_buf(buf, count);
		return count;
	case DEV_TTYBOOT:;
		const char *c = buf;
		while (count > 0) {
			write_serial_char(*c);
			c++;
			count--;
		}
		return count;

	default:
		return -EINVAL;
	}
}

static vfs_fd_ops_t mem_ops = {
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
	dev->type   = DEVICE_CHAR;
	dev->ops    = &mem_ops;
	int ret = register_device(dev);
	if (ret < 0) {
		kfree(dev->name);
		kfree(dev);
	}
	return ret;
}

void init_mem_devices(void) {
	kstatusf("init memory devices ... ");
	register_device_driver(&mem_driver);
	create_mem_dev(DEV_NULL   , "null");
	create_mem_dev(DEV_ZERO   , "zero");
	create_mem_dev(DEV_FULL   , "full");
	create_mem_dev(DEV_KMSG   , "kmsg");
	create_mem_dev(DEV_TTYBOOT, "ttyboot");
	kok();
}
