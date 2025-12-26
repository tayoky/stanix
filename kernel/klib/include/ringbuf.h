#ifndef RINGBUF_H
#define RINGBUF_H

#include <kernel/sleep.h>
#include <kernel/spinlock.h>
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
	char *buffer;
	size_t write_offset;
	size_t read_offset;
	size_t buffer_size;
	size_t read_available;
	sleep_queue reader_queue;
	sleep_queue writer_queue;
	spinlock lock;
} ring_buffer;

ring_buffer *new_ringbuffer(size_t buffer_size);
void delete_ringbuffer(ring_buffer *ring);
ssize_t ringbuffer_read(void *buf,ring_buffer *ring,size_t count);
ssize_t ringbuffer_write(const void *buf,ring_buffer *ring,size_t count);
size_t ringbuffer_read_available(ring_buffer *ring);
size_t ringbuffer_write_available(ring_buffer *ring);

#endif
