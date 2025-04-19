#include <kernel/pipe.h>
#include <kernel/ringbuf.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <errno.h>
#include <kernel/print.h>

struct pipe{
	ring_buffer ring;
	uint64_t reader_count;
	uint64_t writer_count;
};

#define PIPE_SIZE 4096

int create_pipe(vfs_node **read,vfs_node **write){
	struct pipe *pipe_inode = kmalloc(sizeof(struct pipe));
	pipe_inode->reader_count = 1;
	pipe_inode->writer_count = 1;
	pipe_inode->ring = new_ringbuffer(PIPE_SIZE);

	*read  = kmalloc(sizeof(vfs_node));
	*write = kmalloc(sizeof(vfs_node));

	//reset
	memset(*read ,0,sizeof(vfs_node));
	memset(*write,0,sizeof(vfs_node));
	
	//set the inodes
	(*read)->private_inode  = pipe_inode;
	(*write)->private_inode = pipe_inode;

	//set functions
	(*read)->close  = pipe_close;
	(*write)->close = pipe_close;
	(*read)->read   = pipe_read;
	(*write)->write = pipe_write;
	return 0;
}

ssize_t pipe_read(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	struct pipe *pipe_inode = (struct pipe *)node->private_inode;

	//borken pipe check
	if(!pipe_inode->writer_count){
		return -EPIPE;
	}

	return ringbuffer_read(buffer,&pipe_inode->ring,count);
}

ssize_t pipe_write(vfs_node *node,void *buffer,uint64_t offset,size_t count){
	(void)offset;
	struct pipe *pipe_inode = (struct pipe *)node->private_inode;

	//borken pipe check
	if(!pipe_inode->reader_count){
		return -EPIPE;
	}

	return ringbuffer_write(buffer,&pipe_inode->ring,count);
}

void pipe_close(vfs_node *node){
	struct pipe *pipe_inode = (struct pipe *)node->private_inode;

	//determinate if we are a read pipe
	char is_read_pipe = node->read != 0;

	//if we are read decrease reader count else writer count
	if(is_read_pipe){
		pipe_inode->reader_count--;
	} else {
		pipe_inode->writer_count--;
	}

	//if nobody read or write this delete it
	if(pipe_inode->writer_count == 0 && pipe_inode->reader_count == 0){
		delete_ringbuffer(&pipe_inode->ring);
		kfree(pipe_inode);
	}

	return;
}
