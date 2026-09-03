#ifndef KERNEL_BLOCK_H
#define KERNEL_BLOCK_H

#include <kernel/list.h>
#include <kernel/device.h>
#include <kernel/mutex.h>
#include <kernel/assert.h>
#include <kernel/cache.h>
#include <kernel/ioreq.h>
#include <kernel/iobuf.h>

typedef struct block_ops block_ops_t;
typedef struct block_device block_device_t;
typedef struct block_request block_request_t;
typedef struct block_partition block_partition_t;
typedef struct block_partition_driver block_partition_driver_t;

struct block_ops {
	int (*submit)(block_device_t *block_device, block_request_t *request);
	int (*ioctl)(block_device_t *block_device, long request, void *arg);
	void (*cleanup)(block_device_t *block_device);
};

struct block_device {
	device_t device;
	cache_t cache;
	mutex_t mutex;
	char uuid[64];
	block_ops_t *ops;
	block_partition_driver_t *part_driver; // protected by mutex
	list_t partitions;       // protected by mutex
	size_t partitions_count; // protected by mutex
	void *part_data;
	size_t sector_size;
	size_t sectors_count;
	spinlock_t lock;
	int unplugged;      // protected by lock and write protected by mutex
};

struct block_request {
	ioreq_t ioreq;
	iobuf_t iobuf;
	block_device_t *block_device;
	size_t start_sector;
	size_t sectors_count;
	size_t index;
	int type;
};

#define BLOCK_REQUEST_READ  1
#define BLOCK_REQUEST_WRITE 2
#define BLOCK_REQUEST_FLUSH 3

struct block_partition {
	list_node_t node;
	device_t device;
	char uuid[64];
	char fs_uuid[64];
	block_device_t *block_device;
	off_t offset;
	size_t size;
	size_t index;
	int unplugged; // protected by lock
	spinlock_t lock;
};

struct block_partition_driver {
	list_node_t node;
	const char *name;
	int (*probe)(block_device_t *block_device);
	int (*attach)(block_device_t *block_device);
	void (*detach)(block_device_t *block_device);
};

void init_block(void);

block_request_t *block_create_request(block_device_t *block_device, int type);

static inline int block_device_is_unplugged(block_device_t *block_device) {
	spinlock_acquire(&block_device->lock);
	int unplugged = block_device->unplugged;
	spinlock_release(&block_device->lock);
	return unplugged;
}

static inline block_device_t *block_device_ref(block_device_t *block_device) {
	if (block_device) device_ref(&block_device->device);
	return block_device;
}

static inline void block_device_release(block_device_t *block_device) {
	if (block_device) device_release(&block_device->release);
}

int block_device_register(block_device_t *block_device, const char *fmt, dev_t number);
ssize_t block_device_read(block_device_t *block_device, void *buf, off_t offset, size_t count);
ssize_t block_device_write(block_device_t *block_device, const void *buf, off_t offset, size_t count);
int block_device_ioctl(block_device_t *block_device, long request, void *arg);
int block_device_flush(block_device_t *block_device, off_t offset, size_t count);
int block_device_rescan_partitions(block_device_t *block_device);
int block_device_add_partition(block_device_t *block_device, off_t offset, size_t size, const char *uuid, const char *fs_uuid);

static inline int block_partition_is_unplugged(block_partition_t *partition) {
	spinlock_acquire(&partition->lock);
	int unplugged = partition->unplugged;
	spinlock_release(&partition->lock);
	return unplugged;
}

int block_partition_driver_register(block_partition_driver_t *driver);
int block_partition_driver_unregister(block_partition_driver_t *driver);
#endif
