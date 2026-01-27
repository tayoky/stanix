#include <kernel/ringbuf.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/scheduler.h>
#include <kernel/sleep.h>
#include <errno.h>

void init_ringbuffer(ringbuffer_t *ring, size_t buffer_size) {
	memset(ring, 0, sizeof(ringbuffer_t));
	ring->buffer_size = buffer_size;
	ring->write_offset = 0;
	ring->read_offset = 0;
	ring->read_available = 0;
	ring->buffer = kmalloc(buffer_size);
}

void destroy_ringbuffer(ringbuffer_t *ring) {
	kfree(ring->buffer);
}

size_t ringbuffer_read_available(ringbuffer_t *ring) {
	return ring->read_available;
}

size_t ringbuffer_write_available(ringbuffer_t *ring) {
	// take the buffer size and take what is used
	return ring->buffer_size - ringbuffer_read_available(ring);
}

ssize_t ringbuffer_read(ringbuffer_t *ring, void *buf, size_t count, long flags) {
	spinlock_acquire(&ring->lock);

	// check if there are something to read or sleep
	if (ringbuffer_read_available(ring) == 0) {
		if (flags & O_NONBLOCK) {
			spinlock_release(&ring->lock);
			return -EWOULDBLOCK;
		}
		if (sleep_on_queue_lock(&ring->reader_queue, ringbuffer_read_available(ring), &ring->lock) == -EINTR) {
			return -EINTR;
		}
	}

	char *buffer = (char *)buf;
	// cant read more that what is available
	if (count > ringbuffer_read_available(ring)) {
		count = ringbuffer_read_available(ring);
	}
	ring->read_available -= count;

	// if the read go farther than the end cut in two
	size_t rest_count = count;
	if (count + ring->read_offset >= ring->buffer_size) {
		memcpy(buffer, ring->buffer + ring->read_offset, ring->buffer_size - ring->read_offset);
		rest_count -= ring->buffer_size - ring->read_offset;
		buffer += ring->buffer_size - ring->read_offset;
		ring->read_offset = 0;
	}

	// now read the rest
	memcpy(buffer, ring->buffer + ring->read_offset, rest_count);
	ring->read_offset += rest_count;

	wakeup_queue(&ring->writer_queue, 1);
	spinlock_release(&ring->lock);

	return count;
}

ssize_t ringbuffer_write(ringbuffer_t *ring, const void *buf, size_t count, long flags) {
	char *buffer = (char *)buf;

	while (count) {
		spinlock_acquire(&ring->lock);

		if (ringbuffer_write_available(ring) == 0) {
			if (flags & O_NONBLOCK) {
				spinlock_release(&ring->lock);
				return -EWOULDBLOCK;
			}
			if (sleep_on_queue_lock(&ring->writer_queue, ringbuffer_write_available(ring), &ring->lock) == -EINTR) {
				return -EINTR;
			}
		}

		// cant write more that what is available
		size_t rest_count = count;
		if (rest_count > ringbuffer_write_available(ring)) {
			rest_count = ringbuffer_write_available(ring);
		}

		count -= rest_count;
		ring->read_available += rest_count;

		// if the write go farther than the end cut in two
		if (rest_count + ring->write_offset >= ring->buffer_size) {
			memcpy(ring->buffer + ring->write_offset, buffer, ring->buffer_size - ring->write_offset);
			rest_count -= ring->buffer_size - ring->write_offset;
			buffer += ring->buffer_size - ring->write_offset;
			ring->write_offset = 0;
		}

		// now write the rest
		memcpy(ring->buffer + ring->write_offset, buffer, rest_count);
		ring->write_offset += rest_count;
		buffer += rest_count;

		// if a process is waiting to read wakeup
		wakeup_queue(&ring->reader_queue, 1);
		spinlock_release(&ring->lock);
	}


	return count;
}