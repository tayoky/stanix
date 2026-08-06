#include <kernel/userspace.h>
#include <kernel/kheap.h>
#include <kernel/slab.h>
#include <kernel/cond.h>
#include <kernel/device.h>
#include <kernel/block.h>
#include <sys/block.h>
#include <errno.h>

static slab_cache_t block_requests_slab;

void init_block(void) {
	slab_init(&block_requests_slab, sizeof(block_request_t), "block-requests");
}

// TODO : expose an async API (when the vfs support one)
static ssize_t do_request(block_device_t *block_device, void *buf, off_t offset, size_t count, int type) {
	if (device_is_unplugged(&block_device->device)) {
		return -ENXIO;
	}

	size_t start = offset;
	size_t end   = offset + count;
	size_t start_sector = start / block_device->sector_size;
	size_t end_sector   = (end + block_device->sector_size - 1) / block_device->sector_size;
	size_t sectors_count = end_sector - start_sector;

	// bound checks
	if (start == end) {
		return 0;
	}
	if (start_sector >= block_device->sectors_count) {
		return 0;
	}
	if (end_sector > block_device->sectors_count) {
		end_sector = block_device->sectors_count;
		end = end_sector * block_device->sector_size;
	}

	void *kbuf = NULL;
	if (start % block_device->sector_size != 0
			|| end % block_device->sector_size != 0
			|| (uintptr_t)buf % 16 != 0) {
		kbuf = kmalloc((end_sector - start_sector) * block_device->sector_size);
		if (!kbuf) return -ENOMEM;
		if (type == BLOCK_REQUEST_WRITE) {
			// fill first and last sector
			int ret = 0;
			if (start % block_device->sector_size != 0) {
				block_request_t *request = block_create_request(block_device, BLOCK_REQUEST_READ);
				request->start_sector = start_sector;
				request->sectors_count = 1;
				request->buf = kbuf;
				ret = block_device_sumbit_sync(request);
				if (ret < 0) goto error;
			}
			if (end % block_device->sector_size != 0 && (start % block_device->sector_size == 0 || start_sector != end_sector)) {
				block_request_t *request = block_create_request(block_device, BLOCK_REQUEST_READ);
				request->start_sector = end_sector - 1;
				request->sectors_count = 1;
				request->buf = kbuf + (sectors_count - 1) * block_device->sector_size;
				ret = block_submit_request_sync(request);
				if (ret < 0) goto error;
			}
			ret = safe_copy_from((char*)kbuf + start % block_device->sector_size, buf, end - start);
			if (ret < 0) {
error:
				kfree(kbuf);
				return ret;
			}
		}
	}

	block_request_t *request = block_create_request(block_device, type);
	request->start_sector = start_sector;
	request->sectors_count = sectors_count;
	request->buf = kbuf ? kbuf : buf;
	int ret = block_submit_request_sync(request);
	if (ret >= 0 && type == BLOCK_REQUEST_READ && kbuf) {
		ret = safe_copy_to(buf, (char*)kbuf + start % block_device->sector_size, end - start);
	}

	kfree(kbuf);
	return ret < 0 ? ret : (ssize_t)(end - start);
}

static ssize_t block_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	return do_request(block_device, buffer, offset, count, BLOCK_REQUEST_READ);
}

static ssize_t block_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	return do_request(block_device, (void*)buffer, offset, count, BLOCK_REQUEST_WRITE);
}

static int block_ioctl(vfs_fd_t *fd, long request, void *arg) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	switch (request) {
	case I_BLOCK_GET_SIZE:;
		size_t size = block_device->sectors_count * block_device->sector_size;
		return safe_copy_auto_to(arg, &size);
	default:
		if (block_device->ops->ioctl) {
			return block_device->ops->ioctl(block_device, request, arg);
		}
		return -ENOTTY;
	}
}

static vfs_fd_ops_t block_ops = {
	.read = block_read,
	.write = block_write,
	.ioctl = block_ioctl,
};

block_request *block_create_request(block_device_t *block_device, int type) {
	kassert(block_device);
	block_request_t *request = slab_alloc(&block_requests_slab);
	if (!request) return;
	memset(request, 0, sizeof(block_request_t));
	request->block_device = block_device;
	request->type         = type;
	return request;
}

int block_submit_request(block_request_t *request) {
	kassert(request->block_device);
	kassert(request->block_device->ops);
	if (!request->block_device->ops->request) {
		return -EIO;
	}
	int ret = request->block_device->ops->submit(request->block_device, request);
	if (ret == -EAGAIN)  {
		// the block device cannot handle that many requests
		// we have to try again later when some requests finish
		list_append(&request->block_device.pending_requests, &request->node);
		ret = 0;
	} else if (ret < 0) {
		slab_free(request);
	}
	return ret;
}

typedef struct block_wait_data {
	cond_t cond;
	int ret;
} block_wait_data_t;

static void block_wait_callback(block_request_t *request, void *data) {
	block_wait_data_t *wait_data = data;
	wait_data->ret = request->ret;
	cond_set(&wait_data->cond);
}

int block_submit_request_sync(block_request_t *request) {
	block_wait_data_t wait_data;
	init_cond(&wait_data.cond);

	block_request_set_callback(request, block_wait_callback, &wait_data);
	int ret = block_device_sumbit(request);
	if (ret < 0) return ret;

	int ret = cond_wait(&wait_data.cond);
	if (ret < 0) return ret;
	return wait_data.ret;
}

void block_cancel_request(block_request_t *request) {
	slab_free(request);
}

void block_finish_request(block_request_t *request, int ret) {
	request->ret = ret;
	if (request->callback) {
		request->callback(request, request->data);
	}
	slab_free(request);

	// maybee now the block device can handle a pending request
	block_submit_pending_request();
}

void block_submit_pending_request(block_device_t *block_device);
	if (list_is_empty(&block_device->pending_requests)) {
		return;
	}

	// TODO : IO scheduler
	block_request_t *request = container_of(block_device->pending_request.first_node, block_request_t, node);
	list_remove(&block->device.pending_request, &request->node);
	int ret = block_device->ops->submit(block_device, request);
	if (ret == -EAGAIN) {
		// still not ready
		list_prepend(&block->device.pending_request, &request->node);
	} else if (ret < 0) {
		// UNSAFE : recursion
		block_finish_request(request);
	}
}

static void block_destroy(device_t *device) {
	block_device_t *block_device = container_of(device, block_device_t, device);
	// TODO : cancel every requests
}

static void block_cleanup(device_t *device) {
	block_device_t *block_device = container_of(device, block_device_t, device);
	if (block_device->ops->cleanup) {
		block_device->ops->cleanup(block_device);
	}
}

int block_device_register(block_device_t *block_device, const char *fmt, dev_t number) {
	block_device->device.type    = DEVICE_BLOCK;
	block_device->device.ops     = &block_ops;
	block_device->device.destroy = block_destroy;
	block_device->device.cleanup = block_cleanup;
	return device_register(&block_device->device, fmt, number);
}
