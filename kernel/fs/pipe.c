#include <kernel/kheap.h>
#include <kernel/pipe.h>
#include <kernel/poll.h>
#include <kernel/print.h>
#include <kernel/ringbuf.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/sleep.h>
#include <kernel/vfs.h>
#include <errno.h>
#include <poll.h>
#include <stdatomic.h>

typedef struct pipe {
	ringbuffer_t ring;          // protected by lock
	sleep_queue_t reader_queue; // protected by lock
	sleep_queue_t writer_queue; // protected by lock
	int isbroken;               // protected by lock
	spinlock_t lock;
} pipe_t;

#define PIPE_SIZE 4096

static ssize_t pipe_raw_read(pipe_t *pipe, void *buffer, size_t count, long flags) {
	// broken pipe check
	if (pipe->isbroken && ringbuffer_read_available(&pipe->ring) == 0) {
		return 0;
	}

	if (ringbuffer_read_available(&pipe->ring) == 0) {
		if (flags & O_NONBLOCK) {
			return -EAGAIN;
		} else {
			// wait until read available
			if (sleep_on_queue_lock_interruptible(&pipe->reader_queue, &pipe->lock, ringbuffer_read_available(&pipe->ring) || pipe->isbroken) < 0) {
				return -EINTR;
			}

			// second broken pipe check
			if (pipe->isbroken && ringbuffer_read_available(&pipe->ring) == 0) {
				return 0;
			}
		}
	}

	ssize_t ret = ringbuffer_read(&pipe->ring, buffer, count);
	wakeup_queue(&pipe->writer_queue, 0);
	return ret;
}

static ssize_t pipe_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count) {
	(void)offset;
	pipe_t *pipe = (pipe_t *)fd->private;
	spinlock_acquire(&pipe->lock);
	ssize_t ret = pipe_raw_read(pipe, buffer, count, fd->flags);
	spinlock_release(&pipe->lock);
	return ret;
}

static ssize_t pipe_raw_write(pipe_t *pipe, const char *buffer, size_t count, long flags) {
	// broken pipe check
	if (pipe->isbroken) {
		return -EPIPE;
	}

	ssize_t total = 0;
	int ret = 0;
	while (count > 0) {
		// guarantee atomic write for write under size of 4096
		// mandated by posix
		size_t minimum_write = count < 4096 ? count : 4096;
		if (ringbuffer_write_available(&pipe->ring) < minimum_write) {
			if (flags & O_NONBLOCK) {
				ret = -EAGAIN;
				goto finish;
			}
		} else {
			// sleep until we can write
			if (sleep_on_queue_lock_interruptible(&pipe->reader_queue, &pipe->lock, ringbuffer_write_available(&pipe->ring) >= minimum_write || pipe->isbroken) < 0) {
				ret = -EINTR;
				goto finish;
			}
			// second broken pipe check
			if (pipe->isbroken) {
				ret = -EPIPE;
				goto finish;
			}
		}

		ssize_t written = ringbuffer_write(&pipe->ring, buffer, count);
		wakeup_queue(&pipe->reader_queue, 0);
		count  -= written;
		total  += written;
		buffer += written;
	}

finish:
	if (ret < 0 && total == 0) return ret;
	return total;
}

static ssize_t pipe_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count) {
	(void)offset;
	pipe_t *pipe = (pipe_t *)fd->private;
	spinlock_acquire(&pipe->lock);
	ssize_t ret = pipe_raw_write(pipe, buffer, count, fd->flags);
	spinlock_release(&pipe->lock);
	return ret;
}

static int pipe_poll_add(vfs_fd_t *fd, poll_event_t *event) {
	pipe_t *pipe = (pipe_t *)fd->private;
	spinlock_acquire(&pipe->lock);
	
	// cannot wait on broken pipe
	if (!pipe->isbroken) {
		if (fd->ops->read) {
			sleep_add_to_queue(&pipe->reader_queue);
		} else {
			sleep_add_to_queue(&pipe->writer_queue);
		}
	}

	spinlock_release(&pipe->lock);
	return 0;
}

static int pipe_poll_remove(vfs_fd_t *fd, poll_event_t *event) {
	pipe_t *pipe = (pipe_t *)fd->private;
	spinlock_acquire(&pipe->lock);

	if (fd->ops->read) {
		sleep_remove_from_queue(&pipe->reader_queue);
	} else {
		sleep_remove_from_queue(&pipe->writer_queue);
	}

	spinlock_release(&pipe->lock);
	return 0;
}

static int pipe_poll_get(vfs_fd_t *fd, poll_event_t *event) {
	pipe_t *pipe = (pipe_t *)fd->private;
	spinlock_acquire(&pipe->lock);

	if (pipe->isbroken) {
		event->revents |= POLLHUP;
	}
	if (fd->ops->read && ringbuffer_read_available(&pipe->ring) > 0) {
		event->revents |= POLLIN;
	}
	if (fd->ops->write && ringbuffer_write_available(&pipe->ring) > 0) {
		event->revents |= POLLOUT;
	}

	spinlock_release(&pipe->lock);
	return 0;
}

static void pipe_close(vfs_fd_t *fd) {
	pipe_t *pipe = (pipe_t *)fd->private;
	spinlock_acquire(&pipe->lock);

	if (pipe->isbroken) {
		// if it's already broken delete the pipe
		ringbuffer_destroy(&pipe->ring);
		spinlock_release(&pipe->lock);
		kfree(pipe);
	} else {
		// else wakeup everybody that might be waiting for something
		pipe->isbroken = 1;
		wakeup_queue(&pipe->reader_queue, 0);
		wakeup_queue(&pipe->writer_queue, 0);
		spinlock_release(&pipe->lock);
	}
}

static vfs_fd_ops_t pipe_write_ops = {
	.write       = pipe_write,
	.poll_add    = pipe_poll_add,
	.poll_remove = pipe_poll_remove,
	.poll_get    = pipe_poll_get,
	.close       = pipe_close,
};

static vfs_fd_ops_t pipe_read_ops = {
	.read        = pipe_read,
	.poll_add    = pipe_poll_add,
	.poll_remove = pipe_poll_remove,
	.poll_get    = pipe_poll_get,
	.close       = pipe_close,
};

int pipe_create(vfs_fd_t **read, vfs_fd_t **write) {
	pipe_t *pipe = kmalloc(sizeof(pipe_t));
	if (!pipe) return -ENOMEM;
	pipe->isbroken = 0;
	ringbuffer_init(&pipe->ring, PIPE_SIZE);

	*read = vfs_fd_alloc();
	if (!*read) {
		kfree(pipe);
		return -ENOMEM;
	}
	*write = vfs_fd_alloc();
	if (!*write) {
		vfs_close(*read);
		kfree(pipe);
		return -ENOMEM;
	}

	// set the data
	(*read)->ops      = &pipe_read_ops;
	(*write)->ops     = &pipe_write_ops;
	(*read)->flags    = O_RDONLY;
	(*write)->flags   = O_WRONLY;
	(*read)->type     = S_IFIFO;
	(*write)->type    = S_IFIFO;
	(*read)->private  = pipe;
	(*write)->private = pipe;

	return 0;
}
