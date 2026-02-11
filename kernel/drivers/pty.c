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

	//TODO bring back this
	/*if(pty->slave->ref_count == 1 && !ringbuffer_read_available(pty->output_buffer)){
		//nobody as open the slave and there no data
		return -EIO;
	}*/

	return ringbuffer_read(&pty->output_buffer, buffer, count, fd->flags);
}

static ssize_t pty_master_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	(void)offset;
	pty_t *pty = (pty_t *)fd->private;
	tty_t *tty = pty->slave;

	for (size_t i=0; i<count; i++) {
		tty_input(tty,*(char *)buffer);
		(char *)buffer++;
	}
	return (ssize_t)count;
}

static int pty_master_wait_check(vfs_fd_t *fd, short type) {
	pty_t *pty = (pty_t *)fd->private;
	int events = 0;
	/*TODO : bring back this
	if((type & POLLHUP) && pty->slave->ref_count == 1){
		events |= POLLHUP;
	}*/
	if ((type & POLLIN) && ringbuffer_read_available(&pty->output_buffer)) {
		events |= POLLIN;
	}
	if (type & POLLOUT) {
		//we can alaway write to master pty
		events |= POLLOUT;
	}

	return events;
}

void pty_master_close(vfs_fd_t *fd) {
	pty_t *pty = fd->private;

	// the master close so remove the slave
	destroy_device((device_t*)pty->slave);
	pty_cleanup(pty);
}

static vfs_fd_ops_t pty_master_ops = {
	.read       = pty_master_read,
	.write      = pty_master_write,
	.wait_check = pty_master_wait_check,
	.close      = pty_master_close,
};

static tty_ops_t pty_slave_ops = {
	.out     = pty_output,
};

static device_driver_t pty_driver = {
	.name = "pty driver",
};

int new_pty(vfs_fd_t **master_fd, vfs_fd_t **slave_fd, tty_t **rep){
	pty_t *pty = kmalloc(sizeof(pty_t));
	memset(pty,0,sizeof(pty_t));
	init_ringbuffer(&pty->output_buffer, 4096);

	tty_t *slave = new_tty(NULL);
	pty->slave = slave;
	*rep = slave;
	slave->private_data = pty;
	slave->device.driver = &pty_driver;
	slave->ops = &pty_slave_ops;

	// create the master fd
	(*master_fd) = kmalloc(sizeof(vfs_fd_t));
	memset((*master_fd), 0, sizeof(vfs_fd_t));
	(*master_fd)->private   = pty;
	(*master_fd)->ops       = &pty_master_ops;
	(*master_fd)->ref_count = 1;
	(*master_fd)->type      = VFS_FILE;
	(*master_fd)->flags     = O_RDWR;

	(*slave_fd) = open_device((device_t*)slave, O_RDWR);

	// register and save the slave
	char path[32];
	sprintf(path,"pts/%d",kernel->pty_count);
	slave->device.name = strdup(path);
	if(register_device((device_t*)slave) < 0){
		// TODO : delete tty
		return -ENOENT;
	}

	return kernel->pty_count++;
}

void init_ptys(void) {
	kstatusf("init pty ... ");
	register_device_driver(&pty_driver);
	vfs_mkdir("/dev/pts", 0755);
	kok();
}