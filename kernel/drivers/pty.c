#include <kernel/device.h>
#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/userspace.h>
#include <kernel/process.h>
#include <kernel/ringbuf.h>
#include <kernel/string.h>
#include <kernel/poll.h>
#include <kernel/tty.h>
#include <poll.h>

static int pty_is_disconnected(pty_t *pty) {
	return atomic_load(&pty->slave->tty.device.ref_count) == 1;
}

static ssize_t pty_output(tty_t *tty, const char *buf, size_t count) {
	pty_slave_t *slave = container_of(tty, pty_slave_t, tty);
	pty_t *pty = slave->pty;
	
	ssize_t total = 0;
	ssize_t ret = 0;
	spinlock_acquire(&pty->lock);
	while (count > 0) {
		spinlock_raw_release(&slave->tty.lock);
		if (sleep_on_queue_lock_interruptible(&pty->writer_queue, &pty->lock, ringbuffer_write_available(&pty->output_buffer) > 0 || device_is_unplugged(&tty->device)) < 0) {
			spinlock_raw_acquire(&slave->tty.lock);
			ret = -EINTR;
			break;
		}
		spinlock_raw_acquire(&slave->tty.lock);

		if (device_is_unplugged(&tty->device)) {
			ret = -ENXIO;
			break;
		}

		ret = ringbuffer_write(&pty->output_buffer, buf, count);
		if (ret < 0) break;

		wakeup_queue(&pty->reader_queue, 0);
		
		count -= ret;
		buf += ret;
	}
	spinlock_release(&pty->lock);
	if (ret < 0 && total == 0) return ret;
	return total;
}

static void pty_cleanup(pty_t *pty) {
	ringbuffer_destroy(&pty->output_buffer);
	kfree(pty);
}

static ssize_t pty_master_raw_read(pty_t *pty, void *buffer, size_t count, long flags) {
	if (ringbuffer_read_available(&pty->output_buffer) == 0) {
		if (pty_is_disconnected(pty)) {
			// nobody has open the slave and there is no data
			return -EIO;
		} else if (flags & O_NONBLOCK) {
			return -EAGAIN;
		} else {
			// sleep until we can read
			if (sleep_on_queue_lock_interruptible(&pty->reader_queue, &pty->lock, ringbuffer_read_available(&pty->output_buffer) > 0 || pty_is_disconnected(pty)) < 0) {
				return -EINTR;
			}
			if (pty_is_disconnected(pty) && ringbuffer_read_available(&pty->output_buffer) == 0) {
				// nobody has open the slave and there is no data
				return -EIO;
			}
		}
	}

	ssize_t ret = ringbuffer_read(&pty->output_buffer, buffer, count);
	if (ret >= 0) {
		wakeup_queue(&pty->writer_queue, 0);
	}
	return ret;
}

static ssize_t pty_master_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	pty_t *pty = (pty_t *)fd->private;
	spinlock_acquire(&pty->lock);
	ssize_t ret = pty_master_raw_read(pty, buffer, count, fd->flags);
	spinlock_release(&pty->lock);
	return ret;
}

static ssize_t pty_master_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	(void)offset;
	pty_t *pty = (pty_t *)fd->private;
	pty_slave_t *slave = pty->slave;

	const char *buf = buffer;
	ssize_t total = 0;
	int ret = 0;
	int irq_save = spinlock_acquire_irq(&slave->tty.lock);
	while (count > 0) {	
		char kbuf[128];
		size_t w = sizeof(kbuf) < count ? sizeof(kbuf) : count;
		ret = safe_copy_from(kbuf, buf, w);
		if (ret < 0) break;

		if (ringbuffer_write_available(&slave->tty.input_buffer) == 0) {
			if (pty_is_disconnected(pty)) {
				ret = -EIO;
				break;
			} else if (fd->flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			} else if (sleep_on_queue_lock_interruptible(&pty->slave->tty.writer_queue, &pty->slave->tty.lock, ringbuffer_write_available(&slave->tty.input_buffer) > 0 || pty_is_disconnected(pty)) < 0) {
				ret = -EINTR;
				break;
			}
			
		}

		if (pty_is_disconnected(pty)) {
			ret = -EIO;
			break;
		}

		ret = tty_raw_add_input(&pty->slave->tty, kbuf, w);
		if (ret < 0) break;

		count -= w;
		buf += w;
		total += w;
	}
	spinlock_release_irq(&pty->slave->tty.lock, irq_save);
	if (ret < 0 && total == 0) return ret;
	return total;
}

static int pty_master_ioctl(vfs_fd_t *fd, long request, void *arg) {
	pty_t *pty = (pty_t *)fd->private;
	switch (request) {
	case TIOCGPTPEER:;
		vfs_fd_t *slave_fd = device_open(&pty->slave->tty.device, O_RDWR);
		return add_fd(slave_fd, 0);
	default:
		return tty_do_ioctl(&pty->slave->tty, request, arg);
	};
}

static int pty_master_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	pty_t *pty = (pty_t *)fd->private;

	if (event->events & (POLLIN | POLLHUP)) {
		spinlock_acquire(&pty->lock);
		sleep_add_to_queue(&pty->reader_queue);
		spinlock_release(&pty->lock);
	}

	if (event->events & POLLOUT) {
		int irq_save = spinlock_acquire_irq(&pty->slave->tty.lock);
		sleep_add_to_queue(&pty->slave->tty.writer_queue);
		spinlock_release_irq(&pty->slave->tty.lock, irq_save);
	}

	return 0;
}

static int pty_master_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	pty_t *pty = (pty_t *)fd->private;

	spinlock_acquire(&pty->lock);
	sleep_remove_from_queue(&pty->reader_queue);
	spinlock_release(&pty->lock);
	
	int irq_save = spinlock_acquire_irq(&pty->slave->tty.lock);
	sleep_remove_from_queue(&pty->slave->tty.writer_queue);
	spinlock_release_irq(&pty->slave->tty.lock, irq_save);

	return 0;
}

static int pty_master_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	pty_t *pty = (pty_t *)fd->private;

	spinlock_acquire(&pty->lock);
	if (pty_is_disconnected(pty)) event->revents |= POLLHUP;
	if (ringbuffer_read_available(&pty->output_buffer) > 0) event->revents |= POLLIN;
	spinlock_release(&pty->lock);

	int irq_save = spinlock_acquire_irq(&pty->slave->tty.lock);
	if (ringbuffer_write_available(&pty->slave->tty.input_buffer) > 0) event->revents |= POLLOUT;
	spinlock_release_irq(&pty->slave->tty.lock, irq_save);

	return 0;
}

void pty_master_close(vfs_fd_t *fd) {
	pty_t *pty = fd->private;

	// the master close so remove the slave
	device_destroy((device_t *)pty->slave);
	pty_cleanup(pty);
}

static vfs_fd_ops_t pty_master_ops = {
	.read        = pty_master_read,
	.write       = pty_master_write,
	.ioctl       = pty_master_ioctl,
	.poll_add    = pty_master_poll_add,
	.poll_remove = pty_master_poll_remove,
	.poll_get    = pty_master_poll_get,
	.close       = pty_master_close,
};

static tty_ops_t pty_slave_ops = {
	.out = pty_output,
};

static int pty_major = 0;

int new_pty(vfs_fd_t **master_fd, vfs_fd_t **slave_fd, tty_t **rep) {
	pty_t *pty = kmalloc(sizeof(pty_t));
	memset(pty, 0, sizeof(pty_t));
	ringbuffer_init(&pty->output_buffer, 4096);

	pty_slave_t *slave = kmalloc(sizeof(pty_slave_t));
	memset(slave, 0, sizeof(tty_t));
	pty->slave         = slave;
	*rep               = &slave->tty;
	slave->pty         = pty;
	slave->tty.ops     = &pty_slave_ops;

	// create the master fd
	(*master_fd)            = vfs_fd_alloc();
	(*master_fd)->private   = pty;
	(*master_fd)->ops       = &pty_master_ops;
	(*master_fd)->ref_count = 1;
	(*master_fd)->type      = S_IFREG;
	(*master_fd)->flags     = O_RDWR;

	// register and save the slave
	if (tty_register(&slave->tty, "pts/%d", makedev(pty_major, 0)) < 0) {
		vfs_close(*master_fd);
		return -ENOENT;
	}

	*slave_fd = device_open(&slave->tty.device, O_RDWR);

	return minor(slave->tty.device.number);
}

void init_ptys(void) {
	kstatusf("init pty ... ");
	pty_major = device_allocate_major();
	vfs_mkdir("/dev/pts", 0755);
	kok();
}
