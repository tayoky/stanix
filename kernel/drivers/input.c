#include <kernel/print.h>
#include <kernel/input.h>
#include <kernel/process.h>
#include <kernel/userspace.h>
#include <sys/input.h>
#include <poll.h>

#define check_control(val) if (!input_device->controlling_fd) input_device->controlling_fd = fd;\
										 if (input_device->controlling_fd != fd) return val;

static void input_device_drop_control(input_device_t *input_device) {
	kdebugf("process %d drop control\n", get_current_proc()->pid);
	input_device->controlling_fd = NULL;

	// we need to wakeup everyone
	// because know they can take control of the input device :)
	ringbuffer_wakeup_all(&input_device->events);
}

static int input_device_ioctl(vfs_fd_t *fd, long req, void *arg) {
	int ret = -EINVAL;
	input_device_t *input_device = fd->private;
	if (device_is_unplugged(&input_device->device)) {
		return -ENXIO;
	}
	switch (req) {
	case I_INPUT_GET_CONTROL:
		kdebugf("process %d take control\n", get_current_proc()->pid);
		input_device->controlling_fd = fd;
		return 0;
	case I_INPUT_DROP_CONTROL:
		input_device_drop_control(input_device);
		return 0;
	case I_INPUT_GET_INFO:;
		struct input_info info;
		info.if_class    = input_device->class;
		info.if_subclass = input_device->subclass;
		return safe_copy_auto_to(arg, &info);

		// allow layout only on keyboards
	case I_INPUT_SET_LAYOUT:
		if (input_device->class != IE_CLASS_KEYBOARD) return -EOPNOTSUPP;
		return safe_copy_from(input_device->layout, arg, INPUT_LAYOUT_SIZE);
	case I_INPUT_GET_LAYOUT:
		if (input_device->class != IE_CLASS_KEYBOARD) return -EOPNOTSUPP;
		return safe_copy_to(arg, input_device->layout, INPUT_LAYOUT_SIZE);
	default:
		if (input_device->ops && input_device->ops->ioctl) ret = input_device->ops->ioctl(input_device, req, arg);
		return ret;
	}
}

static ssize_t input_device_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	(void)offset;
	input_device_t *input_device = fd->private;
	if (device_is_unplugged(&input_device->device)) {
		return -ENXIO;
	}
	check_control(0);

	// can only read full events
	count -= count % sizeof(struct input_event);

	return ringbuffer_read(&input_device->events, buf, count, fd->flags);
}

static void input_device_close(vfs_fd_t *fd) {
	input_device_t *input_device = fd->private;
	if (fd == input_device->controlling_fd) {
		input_device_drop_control(input_device);
	}
}

static int input_device_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	input_device_t *input_device = fd->private;
	if (device_is_unplugged(&input_device->device)) {
		// cannot wait un unplugged device
		return 0;
	}
	if (event->events | (POLLIN | POLLHUP)) {
		sleep_add_to_queue(&input_device->events.reader_queue);
	}
	return 0;
}

static int input_device_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	input_device_t *input_device = fd->private;
	if (event->events | (POLLIN | POLLHUP)) {
		sleep_remove_from_queue(&input_device->events.reader_queue);
	}
	return 0;
}

static int input_device_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	input_device_t *input_device = fd->private;
	if (device_is_unplugged(&input_device->device)) {
		event->revents |= POLLHUP;
	}

	check_control(0);

	if (ringbuffer_read_available(&input_device->events)) {
		event->revents |= POLLIN;
	}

	return 0;
}

static void input_device_destroy(device_t *device) {
	input_device_t *input_device = container_of(device, input_device_t, device);
	ringbuffer_wakeup_all(&input_device->events);
	if (input_device->ops && input_device->ops->destroy) {
		input_device->ops->destroy(input_device);
	}
	ringbuffer_destroy(&input_device->events);
}

static void input_device_cleanup(device_t *device) {
	input_device_t *input_device = container_of(device, input_device_t, device);
	if (input_device->ops && input_device->ops->cleanup) {
		input_device->ops->cleanup(input_device);
	}
}

static vfs_fd_ops_t input_ops = {
	.read        = input_device_read,
	.ioctl       = input_device_ioctl,
	.poll_add    = input_device_poll_add,
	.poll_remove = input_device_poll_remove,
	.poll_get    = input_device_poll_get,
	.close       = input_device_close,
};

int input_device_send_event(input_device_t *input_device, struct input_event *event) {
	if (device_is_unplugged(&input_device->device)) {
		return -ENXIO;
	}
	event->ie_class    = input_device->class;
	event->ie_subclass = input_device->subclass;
	ringbuffer_write(&input_device->events, event, sizeof(struct input_event), O_NONBLOCK);
	return 0;
}

int input_device_register(input_device_t *input_device) {
	input_device->device.type    = DEVICE_CHAR;
	input_device->device.ops     = &input_ops;
	input_device->device.destroy = input_device_destroy;
	input_device->device.cleanup = input_device_cleanup;
	ringbuffer_init(&input_device->events, sizeof(struct input_event) * 25);
	return device_register(&input_device->device, NULL, 0);
}
