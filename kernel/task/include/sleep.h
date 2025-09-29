#ifndef _KERNEL_SLEEP_H
#define _KERNEL_SLEEP_H

#include <kernel/spinlock.h>
#include <stddef.h>

struct task;

typedef struct sleep_queue {
	spinlock lock;
	struct task *tail;
	struct task *head;
} sleep_queue;

#endif