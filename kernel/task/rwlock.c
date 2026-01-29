#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/assert.h>
#include <kernel/rwlock.h>
#include <kernel/sleep.h>

// based on an implementation on wikipedia

void init_rwlock(rwlock_t *rwlock) {
	memset(rwlock, 0, sizeof(rwlock_t));
}

int rwlock_acquire_read(rwlock_t *rwlock) {
	spinlock_acquire(&rwlock->lock);
	if (rwlock->writer_active == get_current_task()) {
		// we already own it
		rwlock->lock_depth++;
	} else {
		sleep_on_queue_lock(&rwlock->reader_queue, rwlock->waiting_writers_count == 0 && !rwlock->writer_active, &rwlock->lock);
		rwlock->readers_count++;
	}
	spinlock_release(&rwlock->lock);
	return 0;
}

int rwlock_release_read(rwlock_t *rwlock) {
	spinlock_acquire(&rwlock->lock);
	if (rwlock->writer_active == get_current_task()) {
		// this is a writer lock that aqcuired a read
		rwlock->lock_depth--;
		kassert(rwlock->lock_depth > 0);
	} else {
		rwlock->readers_count--;
		kassert(rwlock->readers_count >= 0);
		if (rwlock->readers_count == 0) {
			wakeup_queue(&rwlock->writer_queue, 1);
		}
	}
	spinlock_release(&rwlock->lock);
	return 0;
}

int rwlock_acquire_write(rwlock_t *rwlock) {
	spinlock_acquire(&rwlock->lock);
	if (rwlock->writer_active == get_current_task()) {
		// we already own it
		rwlock->lock_depth++;
	} else {
		rwlock->waiting_writers_count++;
		sleep_on_queue_lock(&rwlock->writer_queue, rwlock->readers_count == 0 && !rwlock->writer_active, &rwlock->lock);
		rwlock->waiting_writers_count--;
		rwlock->writer_active = get_current_task();
		rwlock->lock_depth = 1;
	}
	spinlock_release(&rwlock->lock);
	return 0;
}

int rwlock_release_write(rwlock_t *rwlock) {
	spinlock_acquire(&rwlock->lock);
	kassert(rwlock->writer_active == get_current_task());
	rwlock->lock_depth--;
	if (rwlock->lock_depth == 0) {
		rwlock->writer_active = NULL;
		if (rwlock->waiting_writers_count > 0) {
			wakeup_queue(&rwlock->writer_queue, 1);
		} else {
			wakeup_queue(&rwlock->reader_queue, 0);
		}
	}
	spinlock_release(&rwlock->lock);
	return 0;
}
