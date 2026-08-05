#include <kernel/userspace.h>
#include <kernel/device.h>
#include <kernel/block.h>
#include <sys/block.h>
#include <errno.h>

static ssize_t do_request(block_device_t *block_device, void *buf, off_t offset, size_t count, int type) {
	if (device_is_unplugged(&block_device->device)) {
		return -ENXIO;
	}

	size_t start = offset;
	size_t end   = offset + count;
	size_t start_sector = start / block_device->sector_size;
	size_t end_sector   = (end + block_device->sector_size - 1) / block_device->sector_size;

	// bound checks
	if (start_sector >= block_device->sectors_count) {
		return 0;
	}
	if (end_sector = block_device->sectors_count) {
		end_sector = block_device->sectors_count;
		end = end_sector * block_device->sector_size;
		return 0;
	}

	void *kbuf = NULL;
	if (start % block_device->sector_size != 0
			|| end % block_device->sector_size != 0
			|| (uintptr_t)buf % 16 != 0) {
		kbuf = kmalloc((end_sector - start_sector) * block_device->sector_size);
		if (!kbuf) return -ENOMEM;
		if (type == BLOCK_REQUEST_WRITE) {
			// TODO : fill first and last sector
			int ret = safe_copy_from((char*)kbuf + start % block_device->sector_size, buf, end - start);
			if (ret < 0) return ret;
		}
	}

	block_request_t request = {
		.start_sector = start_sector,
		.sectors_count = end_sector - start_sector,
		.buf = kbuf ? kbuf : buf,
		.type = type,
	};
	int ret = block_device_request(block_device, &requesy);
	if (ret >= 0 && type == BLOCK_REQUEST_READ && kbuf) {
		ret = safe_copy_to(buf, (char*)kbuf + start % block_device->sector_size, end - start);
	}

	kfree(kbuf);
	return ret < 0 ? ret : end - start;
}

static ssize_t block_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	return do_request(block_device, buffer, offset, count, BLOCK_REQUEST_READ);
}

static ssize_t block_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	return do_request(block_device, (void*)buffer, offset, count, BLOCK_REQUEST_WRITE);
}

static int block_ioctl(vfd_fd_t *fd, long request, void *arg) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	switch (request) {
	case I_BLOCK_GET_SIZE:;
		size_t size = block_device->sectors_count * block_device->sector_size;
		return safe_copy_to_auto(arg, &size);
	default:
		if (block_device->ops->ioctl) {
			return block_device->ops->ioctl(block_device, request, arg);
		}
		return -ENOTTY;
	}
}

static vfs_ops_t block_ops = {
	.read = block_read,
	.write = block_write,
	.ioctl = block_ioctl,
}

int block_device_register(block_device_t *block_device, const char *fmt, dev_t number) {
	block_device->device.type = DEVICE_BLOCK;
	block_device->device.ops  = &block_ops;
	return device_register(&block_device->device, fmt, number);
}
