#ifndef KERNEL_RINGBUF_H
#define KERNEL_RINGBUF_H

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

typedef struct ringbuffer {
	char *buffer;
	size_t write_offset;
	size_t read_offset;
	size_t buffer_size;
	size_t read_available;
} ringbuffer_t;

void ringbuffer_init(ringbuffer_t *ring, size_t buffer_size);
void ringbuffer_destroy(ringbuffer_t *ring);
ssize_t ringbuffer_read(ringbuffer_t *ring, void *buf, size_t count);
ssize_t ringbuffer_write(ringbuffer_t *ring, const void *buf, size_t count);
size_t ringbuffer_read_available(ringbuffer_t *ring);
size_t ringbuffer_write_available(ringbuffer_t *ring);

#endif
