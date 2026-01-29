#ifndef _KERNEL_SLEEP_H
#define _KERNEL_SLEEP_H

#include <kernel/spinlock.h>
#include <kernel/scheduler.h>
#include <kernel/list.h>
#include <stddef.h>

struct task;

typedef struct sleep_queue {
	spinlock_t lock;
	list_t waiters;
} sleep_queue_t;

#define sleep_on_queue_lock(queue, cond, l) ({\
	int ret = 0;\
	for (;;) {\
		/* fast path */\
		if (cond) break;\
\
		block_prepare();\
		spinlock_acquire(&(queue)->lock);\
		if (cond) {\
			block_cancel();\
			spinlock_raw_release(&(queue)->lock);\
			break;\
		}\
		\
		if (l) spinlock_raw_release(l);\
		\
		list_append(&(queue)->waiters, &get_current_task()->waiter_list_node);\
		spinlock_raw_release(&(queue)->lock);\
\
		ret = block_task();\
		if (ret < 0) break;\
		if (l) spinlock_acquire(l);\
	}\
	ret;\
})

#define sleep_on_queue_condition(queue, cond) sleep_on_queue_lock(queue, cond, NULL)

int sleep_on_queue(sleep_queue_t *queue);
void wakeup_queue(sleep_queue_t *queue,size_t count);

#endif