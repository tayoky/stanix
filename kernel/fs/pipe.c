#include <kernel/pipe.h>
#include <kernel/vfs.h>
#include <kernel/ringbuf.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <errno.h>
#include <kernel/print.h>
#include <poll.h>
#include <stdatomic.h>

typedef struct pipe{
	ring_buffer *ring;
	atomic_int isbroken;
} pipe_t;

#define PIPE_SIZE 4096

static ssize_t pipe_read(vfs_fd_t *fd, void *buffer, off_t offset, size_t count){
	(void)offset;
	pipe_t *pipe_inode = (pipe_t *)fd->private;

	//broken pipe check
	if(pipe_inode->isbroken){
		return 0;
	}

	return ringbuffer_read(pipe_inode->ring, buffer, count, fd->flags);
}

static ssize_t pipe_write(vfs_fd_t *fd, const void *buffer, off_t offset, size_t count){
	(void)offset;
	pipe_t *pipe_inode = (pipe_t *)fd->private;

	//broken pipe check
	if(pipe_inode->isbroken){
		return -EPIPE;
	}

	return ringbuffer_write(pipe_inode->ring, buffer, count, fd->flags);
}

static int pipe_wait_check(vfs_fd_t *fd, short type) {
	pipe_t *pipe_inode = (pipe_t *)fd->private;
	int events = 0;
	if(pipe_inode->isbroken){
		events |= POLLHUP;
		//if we are the write end set POLLERR
		if(fd->ops->write){
			events |= POLLERR;
		}
	}
	if((fd->ops->read) && (type & POLLIN) && ringbuffer_read_available(pipe_inode->ring)){
		type |= POLLIN;
	}
	if((fd->ops->write) && (type & POLLOUT) && ringbuffer_write_available(pipe_inode->ring)){
		type |= POLLOUT;
	}
	return type;
}

static void pipe_close(vfs_fd_t *fd) {
	pipe_t *pipe_inode = (pipe_t *)fd->private;

	//if it's aready broken delete the pipe
	if(atomic_exchange(&pipe_inode->isbroken,1)){
		delete_ringbuffer(pipe_inode->ring);
		kfree(pipe_inode);
		return;
	}

	return;
}

static vfs_ops_t pipe_write_ops = {
	.write = pipe_write,
	.wait_check = pipe_wait_check,
	.close = pipe_close,
};

static vfs_ops_t pipe_read_ops = {
	.read = pipe_read,
	.wait_check = pipe_wait_check,
	.close = pipe_close,
};

int create_pipe(vfs_fd_t **read, vfs_fd_t **write) {
	pipe_t *pipe_inode = kmalloc(sizeof(pipe_t));
	pipe_inode->isbroken = 0;
	pipe_inode->ring = new_ringbuffer(PIPE_SIZE);

	*read  = kmalloc(sizeof(vfs_fd_t));
	*write = kmalloc(sizeof(vfs_fd_t));

	//reset
	memset(*read ,0,sizeof(vfs_fd_t));
	memset(*write,0,sizeof(vfs_fd_t));
	(*read)->ref_count  = 1;
	(*write)->ref_count = 1;
	
	//set the data
	(*read)->ops  = &pipe_read_ops;
	(*write)->ops = &pipe_write_ops;
	(*read)->flags  = O_RDONLY;
	(*write)->flags = O_WRONLY;
	(*read)->type  = VFS_FILE;
	(*write)->type = VFS_FILE;
	(*read)->private  = pipe_inode;
	(*write)->private = pipe_inode;

	return 0;
}
