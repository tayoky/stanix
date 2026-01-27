#ifndef RINGBUF_H
#define RINGBUF_H

#include <kernel/sleep.h>
#include <kernel/spinlock.h>
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

typedef struct ringbuffer {
	char *buffer;
	size_t write_offset;
	size_t read_offset;
	size_t buffer_size;
	size_t read_available;
	sleep_queue_t reader_queue;
	sleep_queue_t writer_queue;
	spinlock_t lock;
} ringbuffer_t;

void init_ringbuffer(ringbuffer_t *ring, size_t buffer_size);
void destroy_ringbuffer(ringbuffer_t *ring);
ssize_t ringbuffer_read(ringbuffer_t *ring, void *buf, size_t count, long flags);
ssize_t ringbuffer_write(ringbuffer_t *ring, const void *buf, size_t count, long flags);
size_t ringbuffer_read_available(ringbuffer_t *ring);
size_t ringbuffer_write_available(ringbuffer_t *ring);

#endif
