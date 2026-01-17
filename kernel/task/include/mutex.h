#ifndef MUTEX_H
#define MUTEX_H

#include <kernel/spinlock.h>
#include <kernel/sleep.h>
#include <stddef.h>
#include <stdatomic.h>

struct task;

typedef struct mutex {
	atomic_uintptr_t lock;
	sleep_queue_t sleep_queue;
	size_t depth;
} mutex_t;

void init_mutex(mutex_t *mutex);
int acquire_mutex(mutex_t *mutex);
int try_acquire_mutex(mutex_t *mutex);
void release_mutex(mutex_t *mutex);

#endif