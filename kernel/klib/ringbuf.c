#include <kernel/ringbuf.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/scheduler.h>
#include <kernel/list.h>

ring_buffer new_ringbuffer(size_t buffer_size){
	ring_buffer ring;
	ring.buffer_size = buffer_size;
	ring.write_offset = 0;
	ring.read_offset = 0;
	ring.buffer = kmalloc(buffer_size);
	ring.reader_waiter = new_list();
	return ring;
}

void delete_ringbuffer(ring_buffer *ring){
	free_list(ring->reader_waiter);
	kfree(ring->buffer);
	kfree(ring);
}

size_t ringbuffer_read_available(ring_buffer *ring){
	//read before write
	if(ring->read_offset <= ring->write_offset){
		return (size_t)ring->write_offset - ring->read_offset;
	} else {
		return (size_t)(ring->buffer_size - (ring->write_offset - ring->read_offset));
	}
}

static size_t write_available(ring_buffer *ring){
	//take the buffer size and take what is used
	return ring->buffer_size - ringbuffer_read_available(ring);
}

size_t ringbuffer_read(void *buf,ring_buffer *ring,size_t count){
	//check if there are something to read or sleep
	while(!ringbuffer_read_available(ring)){
		list_append(ring->reader_waiter,get_current_proc());
		block_proc();
	}
	char *buffer = (char *)buf;
	//cant read more that what is available
	if(count > ringbuffer_read_available(ring)){
		count = ringbuffer_read_available(ring);
	}

	//if the read go farther than the end cut in two
	size_t rest_count = count;
	if(count + ring->read_offset >= ring->buffer_size){
		memcpy(buffer,ring->buffer + ring->read_offset,ring->buffer_size - ring->read_offset);
		rest_count -= ring->buffer_size - ring->read_offset;
		buffer += ring->buffer_size - ring->read_offset;
		ring->read_offset = 0;
	}

	//now read the rest
	memcpy(buffer,ring->buffer + ring->read_offset,rest_count);
	ring->read_offset += rest_count;

	return count;
}

size_t ringbuffer_write(void *buf,ring_buffer *ring,size_t count){
	char *buffer = (char *)buf;
	//cant write more that what is available
	if(count > write_available(ring)){
		count = write_available(ring);
	}

	//if the write go farther than the end cut in two
	size_t rest_count = count;
	if(count + ring->write_offset >= ring->buffer_size){
		memcpy(ring->buffer + ring->write_offset,buffer,ring->buffer_size - ring->write_offset);
		rest_count -= ring->buffer_size - ring->write_offset;
		buffer += ring->buffer_size - ring->write_offset;
		ring->write_offset = 0;
	}

	//now write the rest
	memcpy(ring->buffer + ring->write_offset,buffer,rest_count);
	ring->write_offset += rest_count;

	//if process are waiting to read wakeup and reset the queue
	foreach(node,ring->reader_waiter){
		process *proc = node->value;
		unblock_proc(proc);
	}
	free_list(ring->reader_waiter);
	ring->reader_waiter = new_list();

	return count;
}