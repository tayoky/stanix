#include <kernel/ringbuf.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <kernel/scheduler.h>
#include <kernel/sleep.h>
#include <errno.h>

ring_buffer *new_ringbuffer(size_t buffer_size){
	ring_buffer *ring = kmalloc(sizeof(ring_buffer));
	memset(ring,0,sizeof(ring_buffer));
	ring->buffer_size = buffer_size;
	ring->write_offset = 0;
	ring->read_offset = 0;
	ring->buffer = kmalloc(buffer_size);
	return ring;
}

void delete_ringbuffer(ring_buffer *ring){
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

size_t ringbuffer_write_available(ring_buffer *ring){
	//take the buffer size and take what is used
	return ring->buffer_size - ringbuffer_read_available(ring);
}

ssize_t ringbuffer_read(void *buf,ring_buffer *ring,size_t count){
	spinlock_acquire(&ring->lock);

	//check if there are something to read or sleep
	if(ringbuffer_read_available(ring) == 0){
		spinlock_release(&ring->lock);
		//what if things happend between when we release the lock and sleep
		//RACE CONDITION
		if(sleep_on_queue(&ring->reader_queue) == -EINTR){
			return -EINTR;
		}
		//what if somebody acquire lock before us : RACECONDITION
		spinlock_acquire(&ring->lock);
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

	wakeup_queue(&ring->writer_queue,1);
	spinlock_release(&ring->lock);

	return count;
}

ssize_t ringbuffer_write(void *buf,ring_buffer *ring,size_t count){
	char *buffer = (char *)buf;

	
	while(count){
		spinlock_acquire(&ring->lock);

		if(ringbuffer_write_available(ring) == 0){
			spinlock_release(&ring->lock);
			//what if things happend between when we release the lock and sleep
			//RACE CONDITION
			if(sleep_on_queue(&ring->writer_queue) == EINTR){
				return -EINTR;
			}
			//what if somebody acquire lock before us : RACECONDITION
			spinlock_acquire(&ring->lock);
		}

		//cant write more that what is available
		size_t rest_count = count;
		if(rest_count > ringbuffer_write_available(ring)){
			rest_count = ringbuffer_write_available(ring);
		}

		count -= rest_count;

		//if the write go farther than the end cut in two
		if(rest_count + ring->write_offset >= ring->buffer_size){
			memcpy(ring->buffer + ring->write_offset,buffer,ring->buffer_size - ring->write_offset);
			rest_count -= ring->buffer_size - ring->write_offset;
			buffer += ring->buffer_size - ring->write_offset;
			ring->write_offset = 0;
		}

		//now write the rest
		memcpy(ring->buffer + ring->write_offset,buffer,rest_count);
		ring->write_offset += rest_count;
		buffer += rest_count;

		//if a process is waiting to read wakeup
		wakeup_queue(&ring->reader_queue,1);
		spinlock_release(&ring->lock);
	}


	return count;
}