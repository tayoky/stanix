#ifndef MUTEX_H
#define MUTEX_H

#include <kernel/spinlock.h>
#include <kernel/list.h>

typedef struct mutex {
	spinlock lock;
	list waiter;
	int locked;
} mutex_t;

void init_mutex(mutex_t *mutex);
int acquire_mutex(mutex_t *mutex);
int try_acquire_mutex(mutex_t *mutex);
void release_mutex(mutex_t *mutex);

#endif