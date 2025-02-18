#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
	char *buffer;
	uint64_t write_offset;
	uint64_t read_offset;
	size_t buffer_size;
}ring_buffer;

ring_buffer new_ringbuffer(size_t buffer_size);
void delete_ringbuffer(ring_buffer *ring);
size_t ringbuffer_read(void *buf,ring_buffer *ring,size_t count);
size_t ringbuffer_write(void *buf,ring_buffer *ring,size_t count);

#endif