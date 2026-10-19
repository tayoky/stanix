#ifndef KERNEL_MUTEX_H
#define KERNEL_MUTEX_H

#include <kernel/atomic.h>
#include <kernel/sleep.h>
#include <kernel/attributes.h>
#include <stddef.h>

typedef struct mutex {
	ATOMIC(task_t *) lock;
	sleep_queue_t sleep_queue;
	size_t depth;
} ATTR_REENTRANT_CAPABILITY ATTR_CAPABILITY("mutex") mutex_t;

void mutex_init(mutex_t *mutex);
int mutex_acquire(mutex_t *mutex) ATTR_ACQUIRE(mutex);
int mutex_try_acquire(mutex_t *mutex) ATTR_TRY_ACQUIRE(mutex, 1);
void mutex_release(mutex_t *mutex) ATTR_RELEASE(mutex);

#endif
