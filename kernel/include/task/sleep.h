#ifndef _KERNEL_SLEEP_H
#define _KERNEL_SLEEP_H

#include <kernel/spinlock.h>
#include <kernel/list.h>
#include <stddef.h>

struct task;

typedef struct sleep_queue {
	spinlock_t lock;
	list_t waiters;
} sleep_queue_t;


int sleep_on_queue(sleep_queue_t *queue);
void wakeup_queue(sleep_queue_t *queue,size_t count);

#endif