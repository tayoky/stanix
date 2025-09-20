#include <kernel/pipe.h>
#include <kernel/ringbuf.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <errno.h>
#include <kernel/print.h>
#include <poll.h>
#include <stdatomic.h>

struct pipe{
	ring_buffer *ring;
	atomic_int isbroken;
};

#define PIPE_SIZE 4096

int create_pipe(vfs_node **read,vfs_node **write){
	struct pipe *pipe_inode = kmalloc(sizeof(struct pipe));
	pipe_inode->isbroken = 0;
	pipe_inode->ring = new_ringbuffer(PIPE_SIZE);

	*read  = kmalloc(sizeof(vfs_node));
	*write = kmalloc(sizeof(vfs_node));

	//reset
	memset(*read ,0,sizeof(vfs_node));
	memset(*write,0,sizeof(vfs_node));
	(*read)->ref_count  = 1;
	(*write)->ref_count = 1;
	
	//set the inodes
	(*read)->private_inode  = pipe_inode;
	(*write)->private_inode = pipe_inode;

	//set functions
	(*read)->close       = pipe_close;
	(*write)->close      = pipe_close;
	(*read)->read        = pipe_read;
	(*write)->write      = pipe_write;
	(*read)->wait_check  = pipe_wait_check;
	(*write)->wait_check = pipe_wait_check;
	return 0;
}

ssize_t pipe_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	struct pipe *pipe_inode = (struct pipe *)node->private_inode;

	//broken pipe check
	if(pipe_inode->isbroken){
		return 0;
	}

	return ringbuffer_read(buffer,pipe_inode->ring,count);
}

ssize_t pipe_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	struct pipe *pipe_inode = (struct pipe *)node->private_inode;

	//broken pipe check
	if(pipe_inode->isbroken){
		return -EPIPE;
	}

	return ringbuffer_write(buffer,pipe_inode->ring,count);
}

int pipe_wait_check(vfs_node *node,short type){
	struct pipe *pipe_inode = (struct pipe *)node->private_inode;
	int events = 0;
	if(pipe_inode->isbroken){
		events |= POLLHUP;
		//if we are the write end set POLLERR
		if(node->write){
			events |= POLLERR;
		}
	}
	if((node->read) && (type & POLLIN) && ringbuffer_read_available(pipe_inode->ring)){
		type |= POLLIN;
	}
	if((node->write) && (type & POLLOUT) && ringbuffer_write_available(pipe_inode->ring)){
		type |= POLLOUT;
	}
	return type;
}

void pipe_close(vfs_node *node){
	struct pipe *pipe_inode = (struct pipe *)node->private_inode;

	//if it's aready broken delete the pipe
	if(atomic_exchange(&pipe_inode->isbroken,1)){
		delete_ringbuffer(pipe_inode->ring);
		kfree(pipe_inode);
		return;
	}

	return;
}
