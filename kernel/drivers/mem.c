#include <kernel/device.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/earlycon.h>
#include <kernel/kheap.h>
#include <kernel/serial.h>

// memory devices

#define MAJOR_MEM     1
#define MINOR_MEM     1
#define MINOR_NULL    2
#define MINOR_ZERO    5
#define MINOR_FULL    7
#define MINOR_KMSG    11
#define MINOR_TTYBOOT 13

static ssize_t mem_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	(void)offset;
	device_t *device = fd->private;
	switch (minor(device->number)) {
	case MINOR_NULL:
		return 0;
	case MINOR_FULL:
	case MINOR_ZERO:
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
	case MINOR_NULL:
	case MINOR_ZERO:
		return count;
	case MINOR_FULL:
		return -ENOSPC;
	case MINOR_KMSG:
		kprint_buf(buf, count);
		return count;
	case MINOR_TTYBOOT:;
		const char *c = buf;
		earlycon_output_all(c, count);
		return count;

	default:
		return -EINVAL;
	}
}

static vfs_fd_ops_t mem_ops = {
	.read  = mem_read,
	.write = mem_write,
};

static int create_mem_dev(int minor, const char *name) {
	device_t *dev = kmalloc(sizeof(device_t));
	memset(dev, 0, sizeof(device_t));
	dev->type   = DEVICE_CHAR;
	dev->ops    = &mem_ops;
	int ret = device_register(dev, name, makedev(MAJOR_MEM, minor));
	if (ret < 0) {
		kfree(dev->name);
		kfree(dev);
	}
	return ret;
}

void init_mem_devices(void) {
	kstatusf("init memory devices ... ");
	create_mem_dev(MINOR_NULL   , "null");
	create_mem_dev(MINOR_ZERO   , "zero");
	create_mem_dev(MINOR_FULL   , "full");
	create_mem_dev(MINOR_KMSG   , "kmsg");
	create_mem_dev(MINOR_TTYBOOT, "ttyboot");
	kok();
}
