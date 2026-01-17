#ifndef _KERNEL_SLEEP_H
#define _KERNEL_SLEEP_H

#include <kernel/spinlock.h>
#include <stddef.h>

struct task;

typedef struct sleep_queue {
	spinlock_t lock;
	struct task *tail;
	struct task *head;
} sleep_queue_t;


int sleep_on_queue(sleep_queue_t *queue);
void wakeup_queue(sleep_queue_t *queue,size_t count);

#endif