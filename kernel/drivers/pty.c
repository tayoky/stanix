#include <kernel/ringbuf.h>
#include <kernel/kernel.h>
#include <kernel/device.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/tty.h>
#include <poll.h>

static ssize_t pty_output(tty_t *tty, const char *buf, size_t size) {
	pty_t *pty = tty->private_data;
	return ringbuffer_write(&pty->output_buffer, buf, size, 0);
}

static void pty_cleanup(pty_t *pty) {
	destroy_ringbuffer(&pty->output_buffer);
	kfree(pty);
}

static ssize_t pty_master_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	(void)offset;

	pty_t *pty = (pty_t *)fd->private;

	if (atomic_load(&pty->slave->device.ref_count) == 1 && !ringbuffer_read_available(&pty->output_buffer)) {
		//nobody as open the slave and there no data
		return -EIO;
	}

	return ringbuffer_read(&pty->output_buffer, buffer, count, fd->flags);
}

static ssize_t pty_master_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	(void)offset;
	pty_t *pty = (pty_t *)fd->private;
	tty_t *tty = pty->slave;

	for (size_t i=0; i < count; i++) {
		tty_input(tty, *(char *)buffer);
		(char *)buffer++;
	}
	return (ssize_t)count;
}

static int pty_is_disconnected(pty_t *pty) {
	return atomic_load(&pty->slave->device.ref_count) == 1;
}

static int pty_master_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	pty_t *pty = (pty_t *)fd->private;

	// cannot wait on disconnected master
	if (pty_is_disconnected(pty)) {
		return 0;
	}

	if (event->events & (POLLIN | POLLHUP)) {
		sleep_add_to_queue(&pty->output_buffer.reader_queue);
	}
	if (event->events & POLLOUT) {
		sleep_add_to_queue(&pty->slave->input_buffer.writer_queue);
	}

	return 0;
}

static int pty_master_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	pty_t *pty = (pty_t *)fd->private;

	if (event->events & (POLLIN | POLLHUP)) {
		sleep_remove_from_queue(&pty->output_buffer.reader_queue);
	}
	if (event->events & POLLOUT) {
		sleep_remove_from_queue(&pty->slave->input_buffer.writer_queue);
	}

	return 0;
}

static int pty_master_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	pty_t *pty = (pty_t *)fd->private;

	if (ringbuffer_read_available(&pty->output_buffer)) event->revents |= POLLIN;
	if (ringbuffer_write_available(&pty->slave->input_buffer)) event->revents |= POLLOUT;
	if (pty_is_disconnected(pty)) event->revents |= POLLHUP;

	return 0;
}

void pty_master_close(vfs_fd_t *fd) {
	pty_t *pty = fd->private;

	// the master close so remove the slave
	destroy_device((device_t *)pty->slave);
	pty_cleanup(pty);
}

static vfs_fd_ops_t pty_master_ops = {
	.read        = pty_master_read,
	.write       = pty_master_write,
	.poll_add    = pty_master_poll_add,
	.poll_remove = pty_master_poll_remove,
	.poll_get    = pty_master_poll_get,
	.close       = pty_master_close,
};

static tty_ops_t pty_slave_ops = {
	.out     = pty_output,
};

static device_driver_t pty_driver = {
	.name = "pty driver",
};

int new_pty(vfs_fd_t **master_fd, vfs_fd_t **slave_fd, tty_t **rep) {
	pty_t *pty = kmalloc(sizeof(pty_t));
	memset(pty, 0, sizeof(pty_t));
	init_ringbuffer(&pty->output_buffer, 4096);

	tty_t *slave = new_tty(NULL);
	pty->slave = slave;
	*rep = slave;
	slave->private_data = pty;
	slave->device.driver = &pty_driver;
	slave->ops = &pty_slave_ops;

	// create the master fd
	(*master_fd) = vfs_alloc_fd();
	(*master_fd)->private   = pty;
	(*master_fd)->ops       = &pty_master_ops;
	(*master_fd)->ref_count = 1;
	(*master_fd)->type      = VFS_FILE;
	(*master_fd)->flags     = O_RDWR;

	// register and save the slave
	char path[32];
	sprintf(path, "pts/%d", kernel->pty_count);
	slave->device.name = strdup(path);
	if (register_device((device_t *)slave) < 0) {
		// TODO : delete tty
		return -ENOENT;
	}

	// FIXME : maybee there is a better way to do this
	sprintf(path, "/dev/%s", slave->device.name);
	(*slave_fd) = vfs_open(path, O_RDWR);

	return kernel->pty_count++;
}

void init_ptys(void) {
	kstatusf("init pty ... ");
	register_device_driver(&pty_driver);
	vfs_mkdir("/dev/pts", 0755);
	kok();
}