#ifndef MUTEX_H
#define MUTEX_H

#include <kernel/spinlock.h>
#include <stddef.h>

typedef struct mutex {
	spinlock lock;
	int locked;
	struct process_struct *waiter[32];
	size_t waiter_count;
} mutex_t;

void init_mutex(mutex_t *mutex);
int acquire_mutex(mutex_t *mutex);
int try_acquire_mutex(mutex_t *mutex);
void release_mutex(mutex_t *mutex);

#endif