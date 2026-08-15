#include <kernel/print.h>
#include <kernel/input.h>
#include <kernel/process.h>
#include <kernel/userspace.h>
#include <sys/input.h>
#include <poll.h>

static int input_device_check_control(input_device_t *input_device, vfs_fd_t *fd) {
	spinlock_assert_acquired(&input_device->lock);
	if (!input_device->controlling_fd) input_device->controlling_fd = fd;
	return input_device->controlling_fd == fd;
}

static void input_device_drop_control(input_device_t *input_device) {
	spinlock_assert_acquired(&input_device->lock);
	kdebugf("process %d drop control\n", get_current_proc()->pid);
	input_device->controlling_fd = NULL;

	// we need to wakeup everyone
	// because know they can take control of the input device :)
	wakeup_queue(&input_device->sleep_queue);
}

static int input_device_raw_ioctl(input_device_t *input_device, long req, void *arg) {
	int ret = -EINVAL;
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

static int input_device_ioctl(vfs_fd_t *fd, long req, void *arg) {
	input_device_t *input_device = fd->private;
	int irq_save = spinlock_acquire_irq(&input_device->lock);
	int ret = input_device_raw_ioctl(input_device, req, arg);
	spinlock_release_irq(&input_device->lock, irq_save);
	return ret;
}

static ssize_t input_device_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	(void)offset;
	input_device_t *input_device = fd->private;
	int irq_save = spinlock_acquire_irq(&input_device->lock);

	// can only read full events
	count -= count % sizeof(struct input_event);

	ssize_t ret = 0;
	if (device_is_unplugged(&input_device->device)) {
		ret = -ENXIO;
		goto finish;
	}
	if (ringbuffer_read_available(&input_device->events) == 0 || !input_device_check_control(input_device, fd)) {
		if (fd->flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto finish;
		} else {
			// wait until we can read an event
			if (sleep_on_lock_interruptible(&input_device->sleep_queue, &input_device->lock, ringbuffer_read_available(&input_device->events) > 0 && input_device_check_control(input_device)) < 0) {
				ret = -EINTR;
				goto finish;
			}
		}
	}
	ret = ringbuffer_read(&input_device->events, buffer, count);

finish:
	spinlock_release_irq(&input_device->lock, irq_save);
	return ret;
}

static void input_device_close(vfs_fd_t *fd) {
	input_device_t *input_device = fd->private;
	int irq_save = spinlock_acquire_irq(&input_device->lock);
	if (fd == input_device->controlling_fd) {
		input_device_drop_control(input_device);
	}
	spinlock_release_irq(&input_device->lock, irq_save);
}

static int input_device_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	input_device_t *input_device = fd->private;
	int irq_save = spinlock_acquire_irq(&input_device->lock);
	// cannot wait on unplugged device
	if (!device_is_unplugged(&input_device->device)) {
		sleep_add_to_queue(&input_device->sleep_queue);
	}
	spinlock_release_irq(&input_device->lock, irq_save);
	return 0;
}

static int input_device_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	input_device_t *input_device = fd->private;
	int irq_save = spinlock_acquire_irq(&input_device->lock);
	sleep_remove_from_queue(&input_device->sleep_queue);
	spinlock_release_irq(&input_device->lock, irq_save);
	return 0;
}

static int input_device_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	input_device_t *input_device = fd->private;
	int irq_save = spinlock_acquire_irq(&input_device->lock);
	if (device_is_unplugged(&input_device->device)) {
		event->revents |= POLLHUP;
	} else if (input_device_check_control(input_device, fd)) {
		if (ringbuffer_read_available(&input_device->events)) {
			event->revents |= POLLIN;
		}
	}
	spinlock_release_irq(&input_device->lock, irq_save);

	return 0;
}

static void input_device_destroy(device_t *device) {
	input_device_t *input_device = container_of(device, input_device_t, device);
	int irq_save = spinlock_acquire_irq(&input_device->lock);
	if (input_device->ops && input_device->ops->destroy) {
		input_device->ops->destroy(input_device);
	}
	wakeup_queue(&input_device->sleep_queue);
	spinlock_release_irq(&input_device->lock, irq_save);
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
	int irq_save = spinlock_acquire_irq(&input_device->lock);
	if (device_is_unplugged(&input_device->device)) {
		spinlock_release_irq(&input_device->lock, irq_save);
		return -ENXIO;
	}
	event->ie_class    = input_device->class;
	event->ie_subclass = input_device->subclass;
	if (ringbuffer_write(&input_device->events, event, sizeof(struct input_event)) > 0) {
		wakeup_queue(&input_device->sleep_queue);
	}
	spinlock_release_irq(&input_device->lock, irq_save);
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
