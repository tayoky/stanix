#include <kernel/pipe.h>
#include <kernel/vfs.h>
#include <kernel/ringbuf.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/print.h>
#include <kernel/poll.h>
#include <errno.h>
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

static int pipe_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	pipe_t *pipe = (pipe_t *)fd->private;
	if (atomic_load(&pipe->isbroken)) {
		// cannot wait on broken pipe
		return 0;
	}

	return ringbuffer_poll_add(&pipe->ring, event);
}

static int pipe_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	pipe_t *pipe = (pipe_t *)fd->private;
	return ringbuffer_poll_remove(&pipe->ring, event);
}

static int pipe_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	pipe_t *pipe = (pipe_t *)fd->private;
	if (atomic_load(&pipe->isbroken)) {
		event->events |= POLLHUP;
	}
	return ringbuffer_poll_get(&pipe->ring, event);
}

static void pipe_close(vfs_fd_t *fd) {
	pipe_t *pipe = (pipe_t *)fd->private;

	// if it's already broken delete the pipe
	if (atomic_exchange(&pipe->isbroken, 1)) {
		destroy_ringbuffer(&pipe->ring);
		kfree(pipe);
		return;
	} else {
		// else wakeup everybody that might be waiting for something
		ringbuffer_wakeup_all(&pipe->ring);
	}

	return;
}

static vfs_fd_ops_t pipe_write_ops = {
	.write = pipe_write,
	.poll_add    = pipe_poll_add,
	.poll_remove = pipe_poll_remove,
	.poll_get    = pipe_poll_get,
	.close = pipe_close,
};

static vfs_fd_ops_t pipe_read_ops = {
	.read = pipe_read,
	.poll_add    = pipe_poll_add,
	.poll_remove = pipe_poll_remove,
	.poll_get    = pipe_poll_get,
	.close = pipe_close,
};

int create_pipe(vfs_fd_t **read, vfs_fd_t **write) {
	pipe_t *pipe = kmalloc(sizeof(pipe_t));
	pipe->isbroken = 0;
	init_ringbuffer(&pipe->ring, PIPE_SIZE);

	*read  = vfs_alloc_fd();
	*write = vfs_alloc_fd();

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
