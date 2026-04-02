#ifndef _KERNEL_SLEEP_H
#define _KERNEL_SLEEP_H

#include <kernel/spinlock.h>
#include <kernel/scheduler.h>
#include <kernel/list.h>
#include <stddef.h>

typedef struct sleep_queue_node {
	list_node_t node;
	task_t *task;
} sleep_queue_node_t;

typedef struct sleep_queue {
	spinlock_t lock;
	list_t waiters;
} sleep_queue_t;

#define sleep_on_queue_lock_interruptible(queue, cond, l) ({\
	int ret = 0;\
	for (;;) {\
		/* fast path */\
		if (cond) break;\
\
		block_prepare_interruptible();\
		spinlock_acquire(&(queue)->lock);\
		if (cond) {\
			block_cancel();\
			spinlock_raw_release(&(queue)->lock);\
			break;\
		}\
		\
		if (l) spinlock_raw_release(l);\
		\
		sleep_add_to_queue_unlocked(queue);\
		spinlock_raw_release(&(queue)->lock);\
\
		ret = block_task();\
		if (ret < 0) break;\
		if (l) spinlock_acquire(l);\
	}\
	ret;\
})


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
		sleep_add_to_queue_unlocked(queue);\
		spinlock_raw_release(&(queue)->lock);\
\
		ret = block_task();\
		if (ret < 0) break;\
		if (l) spinlock_acquire(l);\
	}\
	ret;\
})

#define sleep_on_queue_condition(queue, cond) sleep_on_queue_lock(queue, cond, NULL)
#define sleep_on_queue_condition_interruptible(queue, cond) sleep_on_queue_lock_interruptible(queue, cond, NULL)

void init_sleep(void);
void sleep_add_to_queue_unlocked(sleep_queue_t *queue);
void sleep_add_to_queue(sleep_queue_t *queue);
int sleep_on_queue(sleep_queue_t *queue);
int sleep_on_queue_interruptible(sleep_queue_t *queue);
void wakeup_queue(sleep_queue_t *queue,size_t count);

#endif