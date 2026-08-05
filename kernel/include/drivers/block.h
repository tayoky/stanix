#ifndef KERNEL_BLOCK_H
#define KERNEL_BLOCK_H

#include <kernel/device.h>
#include <kernel/assert.h>

typedef struct block_ops block_ops_t;
typedef struct block_device block_device_t;
typedef struct block_request block_request_t;

struct block_ops {
	int (*request)(block_device_t *block_device, block_request_t *request);
	int (*ioctl)(block_device_t *block_device, long request, void *arg);
};

struct block_device {
	device_t device;
	block_ops_t *ops;
	size_t sector_size;
	size_t sectors_count;
};

struct block_request {
	size_t start_sector;
	size_t sectors_count;
	void *buf;
	int type;
};

#define BLOCK_REQUEST_READ  1
#define BLOCK_REQUEST_WRITE 2

static inline int block_device_request(block_device_t *block_device, block_request_t *request) {
	kassert(block_device->ops);
	if (block_device->ops->request) {
		return block_device->ops->request(block_device, request);
	}
	return -EIO;
}

int block_device_register(block_device_t *block_device, const char *fmt, dev_t number);
#endif
