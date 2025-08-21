#ifndef MUTEX_H
#define MUTEX_H

#include <kernel/spinlock.h>
#include <stddef.h>

struct process_struct;

typedef struct mutex {
	spinlock lock;
	int locked;
	struct process_struct *waiter_head;
	struct process_struct *waiter_tail;
	size_t waiter_count;
} mutex_t;

void init_mutex(mutex_t *mutex);
int acquire_mutex(mutex_t *mutex);
int try_acquire_mutex(mutex_t *mutex);
void release_mutex(mutex_t *mutex);

#endif