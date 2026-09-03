#ifndef KERNEL_INPUT_H
#define KERNEL_INPUT_H

#include <kernel/device.h>
#include <kernel/ringbuf.h>
#include <kernel/sleep.h>
#include <kernel/vfs.h>
#include <sys/input.h>

struct input_device;

typedef struct input_ops {
	int (*ioctl)(struct input_device *device, long req, void *arg);
	void (*destroy)(struct input_device *device);
	void (*cleanup)(struct input_device *cleanup);
} input_ops_t;

typedef struct input_device {
	device_t device;
	vfs_fd_t *controlling_fd;       // protected by lock
	input_ops_t *ops;
	ringbuffer_t events;            // protected by lock
	sleep_queue_t sleep_queue;      // protected by lock
	char layout[INPUT_LAYOUT_SIZE]; // protected by lock
	spinlock_t lock;
	unsigned long class;
	unsigned long subclass;
	int unplugged;                  // protected by lock
} input_device_t;

static inline int input_device_is_unplugged(input_device_t *input_device) {
	spinlock_assert_acquired(&input_device->lock);
	return input_device->unplugged;
}

int input_device_register(input_device_t *input_device);
int input_device_send_event(input_device_t *input_device, struct input_event *event);

#endif
