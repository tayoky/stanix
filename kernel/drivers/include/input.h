#ifndef _KERNEL_INPUT_H
#define _KERNEL_INPUT_H

#include <kernel/device.h>
#include <kernel/ringbuf.h>
#include <kernel/vfs.h>
#include <input.h>

typedef struct input_device {
	device_t device;
	vfs_fd_t *controlling_fd;
	vfs_ops_t *ops;
	ringbuffer *events;
} input_device_t;

int register_input_device(input_device_t *device);
int send_input_event(input_device_t *device, struct input_event *event);

#endif
