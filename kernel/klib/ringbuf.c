#include "ringbuf.h"
#include "kheap.h"
#include "string.h"

ring_buffer new_ringbuffer(size_t buffer_size){
	ring_buffer ring;
	ring.buffer_size = buffer_size;
	ring.write_offset = 0;
	ring.read_offset = 0;
	ring.buffer = kmalloc(buffer_size);
	return ring;
}

void delete_ringbuffer(ring_buffer *ring){
	kfree(ring->buffer);
	kfree(ring);
}

static size_t read_available(ring_buffer *ring){
	//read before write
	if(ring->read_offset <= ring->write_offset){
		return (size_t)ring->write_offset - ring->read_offset;
	} else {
		return (size_t)(ring->buffer_size - (ring->write_offset - ring->read_offset));
	}
}

static size_t write_available(ring_buffer *ring){
	//take the buffer size and take what is used
	return ring->buffer_size - read_available(ring);
}

size_t ringbuffer_read(void *buf,ring_buffer *ring,size_t count){
	char *buffer = (char *)buf;
	//cant read more that what is available
	if(count > read_available(ring)){
		count = read_available(ring);
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

	return count;
}