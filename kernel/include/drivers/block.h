#ifndef KERNEL_BLOCK_H
#define KERNEL_BLOCK_H

#include <kernel/list.h>
#include <kernel/device.h>
#include <kernel/assert.h>

typedef struct block_ops block_ops_t;
typedef struct block_device block_device_t;
typedef struct block_request block_request_t;

struct block_ops {
	int (*submit)(block_device_t *block_device, block_request_t *request);
	int (*ioctl)(block_device_t *block_device, long request, void *arg);
};

struct block_device {
	device_t device;
	list_t pending_requests;
	block_ops_t *ops;
	size_t sector_size;
	size_t sectors_count;
};

struct block_request {
	list_node_t node;
	block_device_t *block_device;
	size_t start_sector;
	size_t sectors_count;
	void *buf;
	void (*callback)(block_request_t *request, void *data);
	void *data;
	int type;
	int ret;
};

#define BLOCK_REQUEST_READ  1
#define BLOCK_REQUEST_WRITE 2

void init_block(void);
block_request *block_create_request(block_device_t *block_device, int type);
static inline void block_request_set_callback(block_request_t *block_request, void (*callback)(block_request_t*,void*), void *data) {
	block_request->callback = callback;
	block_request->data     = data;
}
int block_submit_request(block_request_t *request);
int block_submit_request_sync(block_request_t *request);
void block_cancel_request(block_request_t *request);
void block_finish_request(block_request_t *request, int ret);
void block_submit_pending_request(block_device_t *block_device);
int block_device_register(block_device_t *block_device, const char *fmt, dev_t number);
#endif
