#include <kernel/print.h>
#include <kernel/input.h>
#include <kernel/scheduler.h>
#include <kernel/userspace.h>
#include <sys/input.h>
#include <poll.h>

#define check_control(val) if (!device->controlling_fd) device->controlling_fd = fd;\
										 if (device->controlling_fd != fd) return val;

static void input_drop_control(input_device_t *device) {
	kdebugf("process %d drop control\n",get_current_proc()->pid);
	device->controlling_fd = NULL;

	// we need to wakeup everyone
	// because know they can take control of the input device :)
	ringbuffer_wakeup_all(&device->events);
}

static int input_ioctl(vfs_fd_t *fd, long req, void *arg) {
	int ret = -EINVAL;
	input_device_t *device = fd->private;
	switch (req) {
	case I_INPUT_GET_CONTROL:
		kdebugf("process %d take control\n",get_current_proc()->pid);
		device->controlling_fd = fd;
		return 0;
	case I_INPUT_DROP_CONTROL:
		input_drop_control(device);
		return 0;
	case I_INPUT_GET_INFO:;
		struct input_info *info = arg;
		info->if_class    = device->class;
		info->if_subclass = device->subclass;
		return 0;

	// allow layout only on keyboards
	case I_INPUT_SET_LAYOUT:
		if (device->class != IE_CLASS_KEYBOARD) return -EOPNOTSUPP;
		return safe_copy_from(device->layout, arg, INPUT_LAYOUT_SIZE);
	case I_INPUT_GET_LAYOUT:
		if (device->class != IE_CLASS_KEYBOARD) return -EOPNOTSUPP;
		return safe_copy_to(arg, device->layout, INPUT_LAYOUT_SIZE);
	default:
		if (device->ops && device->ops->ioctl) ret = device->ops->ioctl(device, req, arg);
		return ret;
	}
}

static ssize_t input_read(vfs_fd_t *fd, void *buf, off_t offset, size_t count) {
	(void)offset;
	input_device_t *device = fd->private;
	check_control(0);

	// can only read full events
	count -= count % sizeof(struct input_event);

	return ringbuffer_read(&device->events, buf, count, fd->flags);
}

static void input_close(vfs_fd_t *fd) {
	input_device_t *device = fd->private;
	if (fd == device->controlling_fd) {
		input_drop_control(device);
	}
}

static int input_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	input_device_t *device = fd->private;
	if (device_is_unplugged(&device->device)) {
		// cannot wait un unplugged device
		return 0;
	}
	if (event->events | (POLLIN | POLLHUP)) {
		sleep_add_to_queue(&device->events.reader_queue);
	}
	return 0;
}

static int input_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	input_device_t *device = fd->private;
	if (event->events | (POLLIN | POLLHUP)) {
		sleep_remove_from_queue(&device->events.reader_queue);
	}
	return 0;
}

static int input_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	input_device_t *device = fd->private;
	if (device_is_unplugged(&device->device)) {
		event->revents |= POLLHUP;
	}

	check_control(0);

	if (ringbuffer_read_available(&device->events)) {
		event->revents |= POLLIN;
	}

	return 0;
}

static void input_destroy(device_t *device) {
	input_device_t *input_device = (input_device_t*)device;
	if (input_device->ops && input_device->ops->destroy) {
		input_device->ops->destroy(input_device);
	}
	destroy_ringbuffer(&input_device->events);
}

static vfs_fd_ops_t input_ops = {
	.read        = input_read,
	.ioctl       = input_ioctl,
	.poll_add    = input_poll_add,
	.poll_remove = input_poll_remove,
	.poll_get    = input_poll_get,
	.close       = input_close,
};

int send_input_event(input_device_t *device, struct input_event *event) {
	event->ie_class    = device->class;
	event->ie_subclass = device->subclass;
	ringbuffer_write(&device->events, event, sizeof(struct input_event), O_NONBLOCK);
	return 0;
}

int register_input_device(input_device_t *device){
	device->device.type    = DEVICE_CHAR;
	device->device.ops     = &input_ops;
	device->device.destroy = input_destroy;
	init_ringbuffer(&device->events, sizeof(struct input_event) * 25);
	return register_device((device_t*)device);
}
