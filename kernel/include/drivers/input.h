#ifndef _KERNEL_INPUT_H
#define _KERNEL_INPUT_H

#include <kernel/device.h>
#include <kernel/ringbuf.h>
#include <kernel/vfs.h>
#include <sys/input.h>

struct input_device;

typedef struct input_ops {
	int (*ioctl)(struct input_device *device, long req, void *arg);
	int (*destroy)(struct input_device *device);
} input_ops_t;

typedef struct input_device {
	device_t device;
	vfs_fd_t *controlling_fd;
	input_ops_t *ops;
	ringbuffer_t events;
    unsigned long class;
    unsigned long subclass;
} input_device_t;

int register_input_device(input_device_t *device);
int send_input_event(input_device_t *device, struct input_event *event);

#endif
