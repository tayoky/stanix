#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/assert.h>
#include <kernel/rwsem.h>
#include <kernel/sleep.h>

// based on an implementation on wikipedia

void init_rwsem(rwsem_t *rwsem) {
	memset(rwsem, 0, sizeof(rwsem_t));
}

int rwsem_acquire_read(rwsem_t *rwsem) {
	spinlock_acquire(&rwsem->lock);
	if (rwsem->writer_active == get_current_task()) {
		// we already own it
		rwsem->lock_depth++;
	} else {
		sleep_on_queue_lock(&rwsem->reader_queue, rwsem->waiting_writers_count == 0 && !rwsem->writer_active, &rwsem->lock);
		rwsem->readers_count++;
	}
	spinlock_release(&rwsem->lock);
	return 0;
}

int rwsem_release_read(rwsem_t *rwsem) {
	spinlock_acquire(&rwsem->lock);
	if (rwsem->writer_active == get_current_task()) {
		// this is a writer lock that aqcuired a read
		rwsem->lock_depth--;
		kassert(rwsem->lock_depth > 0);
	} else {
		rwsem->readers_count--;
		kassert(rwsem->readers_count >= 0);
		if (rwsem->readers_count == 0) {
			wakeup_queue(&rwsem->writer_queue, 1);
		}
	}
	spinlock_release(&rwsem->lock);
	return 0;
}

int rwsem_acquire_write(rwsem_t *rwsem) {
	spinlock_acquire(&rwsem->lock);
	if (rwsem->writer_active == get_current_task()) {
		// we already own it
		rwsem->lock_depth++;
	} else {
		rwsem->waiting_writers_count++;
		sleep_on_queue_lock(&rwsem->writer_queue, rwsem->readers_count == 0 && !rwsem->writer_active, &rwsem->lock);
		rwsem->waiting_writers_count--;
		rwsem->writer_active = get_current_task();
		rwsem->lock_depth = 1;
	}
	spinlock_release(&rwsem->lock);
	return 0;
}

int rwsem_release_write(rwsem_t *rwsem) {
	spinlock_acquire(&rwsem->lock);
	kassert(rwsem->writer_active == get_current_task());
	rwsem->lock_depth--;
	if (rwsem->lock_depth == 0) {
		rwsem->writer_active = NULL;
		if (rwsem->waiting_writers_count > 0) {
			wakeup_queue(&rwsem->writer_queue, 1);
		} else {
			wakeup_queue(&rwsem->reader_queue, 0);
		}
	}
	spinlock_release(&rwsem->lock);
	return 0;
}
