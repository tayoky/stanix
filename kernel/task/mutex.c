#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/mutex.h>
#include <kernel/panic.h>
#include <stdatomic.h>

//FIXME : should we disable interrupt on mutex

void init_mutex(mutex_t *mutex) {
	memset(mutex, 0, sizeof(mutex_t));
}

int mutex_try_acquire(mutex_t *mutex) {
	uintptr_t expected = 0;
	if (atomic_compare_exchange_strong(&mutex->lock, &expected, (uintptr_t)get_current_task())) {
		// we locked the mutex
		mutex->depth = 1;
		return 1;
	}
	
	// maybee we already own it
	if (expected == (uintptr_t)get_current_task()) {
		mutex->depth += 1;
		return 1;
	}
	return 0;
}

int mutex_acquire(mutex_t *mutex) {
	while (sleep_on_queue_condition(&mutex->sleep_queue, mutex_try_acquire(mutex)) == -EINTR);
	return 0;
}

void mutex_release(mutex_t *mutex) {
	mutex->depth--;
	if (mutex->depth == 0) {
		atomic_store(&mutex->lock, 0);
		wakeup_queue(&mutex->sleep_queue, 1);
	}
}
