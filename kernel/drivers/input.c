#include <kernel/print.h>
#include <kernel/input.h>
#include <kernel/scheduler.h>
#include <sys/input.h>
#include <poll.h>

#define check_control(val) if (!device->controlling_fd) device->controlling_fd = fd;\
										 if (device->controlling_fd != fd) return val;

static int input_ioctl(vfs_fd_t *fd, long req, void *arg) {
	int ret = -EINVAL;
	input_device_t *device = fd->private;
	switch (req) {
	case I_INPUT_GET_CONTROL:
		kdebugf("process %d take control\n",get_current_proc()->pid);
		device->controlling_fd = fd;
		return 0;
	case I_INPUT_DROP_CONTROL:
		kdebugf("process %d drop control\n",get_current_proc()->pid);
		device->controlling_fd = NULL;
		return 0;
	default:
		if (device->ops && device->ops->ioctl) ret = device->ops->ioctl(fd, req, arg);
		return ret;
	}
}

static ssize_t input_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	(void)offset;
	input_device_t *device = fd->private;
	check_control(0);

	// can only read full events
	count -= count % sizeof(struct input_event);

	return ringbuffer_read(device->events, buf, count, fd->flags);
}

static int input_wait_check(vfs_fd_t *fd, short events) {
	input_device_t *device = fd->private;
	check_control(0);
	int ret = 0;
	if ((events & POLLIN) && (ringbuffer_read_available(device->events))) ret |= POLLIN;
	return ret;
}

static void input_close(vfs_fd_t *fd) {
	input_device_t *device = fd->private;
	if (device->ops && device->ops->close) device->ops->close(fd);
	if (fd == device->controlling_fd) {
		device->controlling_fd = NULL;
	}
}

static vfs_ops_t input_ops = {
	.read       = input_read,
	.ioctl      = input_ioctl,
	.wait_check = input_wait_check,
	.close      = input_close,
};

int send_input_event(input_device_t *device, struct input_event *event) {
	ringbuffer_write(device->events, event, sizeof(struct input_event), O_NONBLOCK);
	return 0;
}

int register_input_device(input_device_t *device){
	device->device.type = DEVICE_CHAR;
	device->device.ops = &input_ops;
	device->events = new_ringbuffer(sizeof(struct input_event) * 25);
	return register_device((device_t*)device);
}
