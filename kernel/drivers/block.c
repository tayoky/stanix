#include <kernel/userspace.h>
#include <kernel/kheap.h>
#include <kernel/slab.h>
#include <kernel/oneshot.h>
#include <kernel/device.h>
#include <kernel/block.h>
#include <sys/block.h>
#include <errno.h>

static slab_cache_t block_requests_slab;

void init_block(void) {
	slab_init(&block_requests_slab, sizeof(block_request_t), "block-requests");
}

#define DATA_OFFSET 0
#define DATA_COUNT  1

static int block_fill_iobuf(block_device_t *block_device, block_request_t *request, off_t offset, size_t count) {
	size_t size = request->sectors_count * block_device->sector_size;
	uintptr_t start = request->start_sector * block_device->sector_size;
	int ret = iobuf_init_pages(&request->iobuf, NULL, start % PAGE_SIZE, size);
	if (ret < 0) return ret;

	uintptr_t index = offset % block_device->sector_size;
	for (uintptr_t addr = offset; addr < offset + count; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page(&block_device->cache, addr);
		kassert(page != PAGE_INVALID);
		iobuf_set_page(&request->iobuf, index, page);
		index += PAGE_SIZE;
	}
	return 0;
}

static void block_read_pages_callback(ioreq_t *ioreq, void *data) {
	(void)data;
	block_request_t *request = container_of(ioreq, block_request_t, ioreq);
	block_device_t *block_device = request->block_device;
	off_t offset = ioreq->data2[DATA_OFFSET];
	size_t count = ioreq->data2[DATA_COUNT];

	cache_read_terminate(&block_device->cache, offset, count, ioreq->ret);
}

static int block_read_pages(cache_t *cache, off_t offset, size_t count) {
	block_device_t *block_device = container_of(cache, block_device_t, cache);

	// determinate the bounds of the read
	size_t start = offset;
	size_t end   = offset + count;
	size_t start_sector = start / block_device->sector_size;
	size_t end_sector   = (end + block_device->sector_size - 1) / block_device->sector_size;
	if (end_sector > block_device->sectors_count) {
		end_sector = block_device->sectors_count;
		end = end_sector * block_device->sector_size;
	}
	size_t sectors_count = end_sector - start_sector;

	block_request_t *request = block_create_request(block_device, BLOCK_REQUEST_READ);
	if (!request) return -ENOMEM;
	request->start_sector = start_sector;
	request->sectors_count = sectors_count;

	int ret = block_fill_iobuf(block_device, request, offset, count);
	if (ret < 0) {
		ioreq_release(&request->ioreq);
		return ret;
	}

	request->ioreq.data2[DATA_OFFSET] = offset;
	request->ioreq.data2[DATA_COUNT]   = count;
	ioreq_set_callback(&request->ioreq, block_read_pages_callback, NULL);
	return ioreq_submit(&request->ioreq);
}

// TODO : make this async
static int block_write_pages(cache_t *cache, off_t offset, size_t count) {
	block_device_t *block_device = container_of(cache, block_device_t, cache);

	// determinate the bounds of the write
	size_t start = offset;
	size_t end   = offset + count;
	size_t start_sector = start / block_device->sector_size;
	size_t end_sector   = (end + block_device->sector_size - 1) / block_device->sector_size;
	if (end_sector > block_device->sectors_count) {
		end_sector = block_device->sectors_count;
		end = end_sector * block_device->sector_size;
	}
	size_t sectors_count = end_sector - start_sector;

	// TODO : pass pages direcly
	char *buffer = kmalloc(sectors_count * block_device->sector_size);
	if (!buffer) return -ENOMEM;

	// fill first and last sector
	int ret = 0;
	if (start % block_device->sector_size != 0) {
		block_request_t *request = block_create_request(block_device, BLOCK_REQUEST_READ);
		if (!request) goto error_nomem;
		request->start_sector = start_sector;
		request->sectors_count = 1;
		iobuf_init_continuous(&request->iobuf, buffer, block_device->sector_size);
		ret = ioreq_submit_sync_interruptible(&request->ioreq);
		if (ret < 0) goto error;
	}
	if (end % block_device->sector_size != 0 && (start % block_device->sector_size == 0 || start_sector != end_sector)) {
		block_request_t *request = block_create_request(block_device, BLOCK_REQUEST_READ);
		if (!request) goto error_nomem;
		request->start_sector = end_sector - 1;
		request->sectors_count = 1;
		iobuf_init_continuous(&request->iobuf, buffer + (sectors_count - 1) * block_device->sector_size, block_device->sector_size);
		ret = ioreq_submit_sync_interruptible(&request->ioreq);
		if (ret < 0) goto error;
	}

	char *ptr = buffer;
	ptr += start % block_device->sector_size;
	for (uintptr_t addr = offset; addr < offset + count; addr += PAGE_SIZE) {
		uintptr_t page = cache_lookup_page(cache, addr);
		kassert(page != PAGE_INVALID);
		void *vaddr = mmu_phys2virt(page);
		memcpy(ptr, vaddr, PAGE_SIZE);
		ptr += PAGE_SIZE;
	}

	block_request_t *request = block_create_request(block_device, BLOCK_REQUEST_WRITE);
	if (!request) {
error_nomem:
		ret = -ENOMEM;
error:
		kfree(buffer);
		return ret;
	}
	request->start_sector = start_sector;
	request->sectors_count = sectors_count;
	iobuf_init_continuous(&request->iobuf, buffer, sectors_count * block_device->sector_size);
	ret = ioreq_submit_sync_interruptible(&request->ioreq);
	kfree(buffer);
	if (ret < 0) return ret;

	// we need to flush
	block_request_t *flush_request = block_create_request(block_device, BLOCK_REQUEST_FLUSH);
	if (!flush_request)
	flush_request->start_sector = start_sector;
	flush_request->sectors_count = sectors_count;
	ret = ioreq_submit_sync_interruptible(&flush_request->ioreq);
	if (ret < 0) return ret;

	cache_write_terminate(cache, offset, count, 0);
	return 0;
}

static cache_ops_t block_cache_ops = {
	.read  = block_read_pages,
	.write = block_write_pages,
};

static ssize_t block_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	return cache_read(&block_device->cache, buffer, offset, count);
}

static ssize_t block_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	return cache_write(&block_device->cache, buffer, offset, count);
}

static off_t block_seek(vfs_fd_t *fd, off_t offset, int whence) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);

	off_t new_offset;
	switch (whence) {
	case SEEK_SET:
		new_offset = offset;
		break;
	case SEEK_CUR:
		new_offset = fd->offset + offset;
		break;
	case SEEK_END:
		new_offset = block_device->sectors_count * block_device->sector_size + offset;
		break;
	default:
		return -EINVAL;
	}

	if (new_offset < 0) return -EINVAL;
	return fd->offset = new_offset;
}

static int block_ioctl(vfs_fd_t *fd, long request, void *arg) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	switch (request) {
	case BLOCK_GET_SIZE:;
		size_t size = block_device->sectors_count * block_device->sector_size;
		return safe_copy_auto_to(arg, &size);
	default:
		if (block_device->ops->ioctl) {
			return block_device->ops->ioctl(block_device, request, arg);
		}
		return -ENOTTY;
	}
}

static int block_flush(vfs_fd_t *fd, off_t offset, size_t count) {
	block_device_t *block_device = container_of(fd->private, block_device_t, device);
	return cache_flush(&block_device->cache, offset, count);
}

static vfs_fd_ops_t block_ops = {
	.read  = block_read,
	.write = block_write,
	.seek  = block_seek,
	.ioctl = block_ioctl,
	.flush = block_flush,
};

static int block_submit_request(ioreq_t *ioreq) {
	block_request_t *request = container_of(ioreq, block_request_t, ioreq);
	kassert(request->block_device);
	kassert(request->block_device->ops);
	if (!request->block_device->ops->submit) {
		return -EIO;
	}
	int ret = request->block_device->ops->submit(request->block_device, request);
	return ret;
}

static void block_cancel_request(ioreq_t *ioreq) {
	block_request_t *request = container_of(ioreq, block_request_t, ioreq);
	(void)request;
	// TODO
}

static void block_cleanup_request(ioreq_t *ioreq) {
	block_request_t *request = container_of(ioreq, block_request_t, ioreq);
	iobuf_destroy(&request->iobuf);
	slab_free(request);
}

static ioreq_ops_t block_request_ops = {
	.submit  = block_submit_request,
	.cancel  = block_cancel_request,
	.cleanup = block_cleanup_request,
};

block_request_t *block_create_request(block_device_t *block_device, int type) {
	kassert(block_device);
	block_request_t *request = slab_alloc(&block_requests_slab);
	if (!request) return NULL;
	memset(request, 0, sizeof(block_request_t));
	request->ioreq.ops    = &block_request_ops;
	request->block_device = block_device;
	request->type         = type;
	return request;
}

static void block_destroy(device_t *device) {
	block_device_t *block_device = container_of(device, block_device_t, device);
	(void)block_device;
	// TODO : cancel every requests
	free_cache(&block_device->cache);
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
	init_cache(&block_device->cache);
	block_device->cache.ops      = &block_cache_ops;
	block_device->cache.size     = block_device->sectors_count * block_device->sector_size;
	return device_register(&block_device->device, fmt, number);
}
