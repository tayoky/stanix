#include <kernel/pipe.h>
#include <kernel/vfs.h>
#include <kernel/ringbuf.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <errno.h>
#include <kernel/print.h>
#include <poll.h>
#include <stdatomic.h>

typedef struct pipe {
	ringbuffer_t ring;
	atomic_int isbroken;
} pipe_t;

#define PIPE_SIZE 4096

static ssize_t pipe_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	(void)offset;
	pipe_t *pipe = (pipe_t *)fd->private;

	// broken pipe check
	if (pipe->isbroken && !ringbuffer_read_available(&pipe->ring)) {
		return 0;
	}

	return ringbuffer_read(&pipe->ring, buffer, count, fd->flags);
}

static ssize_t pipe_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	(void)offset;
	pipe_t *pipe = (pipe_t *)fd->private;

	// broken pipe check
	if (pipe->isbroken) {
		return -EPIPE;
	}

	return ringbuffer_write(&pipe->ring, buffer, count, fd->flags);
}

static int pipe_wait_check(vfs_fd_t *fd, short type) {
	pipe_t *pipe = (pipe_t *)fd->private;
	int events = 0;
	if (pipe->isbroken) {
		events |= POLLHUP;
		//if we are the write end set POLLERR
		if (fd->ops->write) {
			events |= POLLERR;
		}
	}
	if ((fd->ops->read) && (type & POLLIN) && ringbuffer_read_available(&pipe->ring)) {
		type |= POLLIN;
	}
	if ((fd->ops->write) && (type & POLLOUT) && ringbuffer_write_available(&pipe->ring)) {
		type |= POLLOUT;
	}
	return type;
}

static void pipe_close(vfs_fd_t *fd) {
	pipe_t *pipe = (pipe_t *)fd->private;

	// if it's aready broken delete the pipe
	if (atomic_exchange(&pipe->isbroken, 1)) {
		destroy_ringbuffer(&pipe->ring);
		kfree(pipe);
		return;
	}

	return;
}

static vfs_fd_ops_t pipe_write_ops = {
	.write = pipe_write,
	.wait_check = pipe_wait_check,
	.close = pipe_close,
};

static vfs_fd_ops_t pipe_read_ops = {
	.read = pipe_read,
	.wait_check = pipe_wait_check,
	.close = pipe_close,
};

int create_pipe(vfs_fd_t **read, vfs_fd_t **write) {
	pipe_t *pipe = kmalloc(sizeof(pipe_t));
	pipe->isbroken = 0;
	init_ringbuffer(&pipe->ring, PIPE_SIZE);

	*read  = kmalloc(sizeof(vfs_fd_t));
	*write = kmalloc(sizeof(vfs_fd_t));

	//reset
	memset(*read, 0, sizeof(vfs_fd_t));
	memset(*write, 0, sizeof(vfs_fd_t));
	(*read)->ref_count  = 1;
	(*write)->ref_count = 1;

	//set the data
	(*read)->ops  = &pipe_read_ops;
	(*write)->ops = &pipe_write_ops;
	(*read)->flags  = O_RDONLY;
	(*write)->flags = O_WRONLY;
	(*read)->type  = VFS_FILE;
	(*write)->type = VFS_FILE;
	(*read)->private  = pipe;
	(*write)->private = pipe;

	return 0;
}
