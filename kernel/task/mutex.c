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

int try_acquire_mutex(mutex_t *mutex) {
	uintptr_t expected = NULL;
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

int acquire_mutex(mutex_t *mutex) {
	while (!try_acquire_mutex(mutex)) {
		// register on the list
		sleep_on_queue(&mutex->sleep_queue);
	}
	
	return 0;
}

void release_mutex(mutex_t *mutex) {
	mutex->depth--;
	if (mutex->depth == 0) {
		atomic_store(&mutex->lock, 0);
		wakeup_queue(&mutex->sleep_queue, 1);
	}
}
