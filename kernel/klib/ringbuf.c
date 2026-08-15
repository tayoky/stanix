#include <kernel/ringbuf.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/scheduler.h>
#include <kernel/userspace.h>
#include <kernel/sleep.h>
#include <kernel/poll.h>
#include <errno.h>

void ringbuffer_init(ringbuffer_t *ring, size_t buffer_size) {
	memset(ring, 0, sizeof(ringbuffer_t));
	ring->buffer_size = buffer_size;
	ring->write_offset = 0;
	ring->read_offset = 0;
	ring->read_available = 0;
	ring->buffer = kmalloc(buffer_size);
}

void ringbuffer_destroy(ringbuffer_t *ring) {
	kfree(ring->buffer);
}

size_t ringbuffer_read_available(ringbuffer_t *ring) {
	return ring->read_available;
}

size_t ringbuffer_write_available(ringbuffer_t *ring) {
	// take the buffer size and take what is used
	return ring->buffer_size - ringbuffer_read_available(ring);
}

ssize_t ringbuffer_read(ringbuffer_t *ring, void *buf, size_t count) {
	char *buffer = (char *)buf;
	// cant read more that what is available
	if (count > ringbuffer_read_available(ring)) {
		count = ringbuffer_read_available(ring);
	}
	ring->read_available -= count;

	// if the read go farther than the end cut in two
	size_t rest_count = count;
	if (count + ring->read_offset >= ring->buffer_size) {
		if (safe_copy_to(buffer, ring->buffer + ring->read_offset, ring->buffer_size - ring->read_offset) < 0) {
			return -EFAULT;
		}
		rest_count -= ring->buffer_size - ring->read_offset;
		buffer += ring->buffer_size - ring->read_offset;
		ring->read_offset = 0;
	}

	// now read the rest
	if (safe_copy_to(buffer, ring->buffer + ring->read_offset, rest_count) < 0){
		return -EFAULT;
	}
	ring->read_offset += rest_count;

	return count;
}

ssize_t ringbuffer_write(ringbuffer_t *ring, const void *buf, size_t count) {
	char *buffer = (char *)buf;

	// cant write more that what is available
	if (count > ringbuffer_write_available(ring)) {
		count = ringbuffer_write_available(ring);
	}

	ring->read_available += count;
	
	// if the write go farther than the end cut in two
	size_t rest_count = count;
	if (count + ring->write_offset >= ring->buffer_size) {
		if (safe_copy_from(ring->buffer + ring->write_offset, buffer, ring->buffer_size - ring->write_offset) < 0) {
			return -EFAULT;
		}
		rest_count -= ring->buffer_size - ring->write_offset;
		buffer += ring->buffer_size - ring->write_offset;
		ring->write_offset = 0;
	}

	// now write the rest
	if (safe_copy_from(ring->buffer + ring->write_offset, buffer, rest_count)) {
		return -EFAULT;
	}
	ring->write_offset += rest_count;

	return count;
}
