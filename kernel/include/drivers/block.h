#ifndef KERNEL_BLOCK_H
#define KERNEL_BLOCK_H

#include <kernel/list.h>
#include <kernel/device.h>
#include <kernel/assert.h>
#include <kernel/cache.h>
#include <kernel/ioreq.h>

typedef struct block_ops block_ops_t;
typedef struct block_device block_device_t;
typedef struct block_request block_request_t;

struct block_ops {
	int (*submit)(block_device_t *block_device, block_request_t *request);
	int (*ioctl)(block_device_t *block_device, long request, void *arg);
	void (*cleanup)(block_device_t *block_device);
};

struct block_device {
	device_t device;
	cache_t cache;
	block_ops_t *ops;
	size_t sector_size;
	size_t sectors_count;
};

struct block_request {
	ioreq_t ioreq;
	block_device_t *block_device;
	size_t start_sector;
	size_t sectors_count;
	void *buf;
	int type;
};

#define BLOCK_REQUEST_READ  1
#define BLOCK_REQUEST_WRITE 2
#define BLOCK_REQUEST_FLUSH 3

void init_block(void);
block_request_t *block_create_request(block_device_t *block_device, int type);
int block_device_register(block_device_t *block_device, const char *fmt, dev_t number);
#endif
