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

/**
 * @brief add the current thread to a sleep queue without locking
 * @warning very dangerous for internal use only
 * @param queue to the queue to add the current task to
 */
void sleep_add_to_queue_unlocked(sleep_queue_t *queue);

/**
 * @brief add the current task to a sleep queue
 * @note if sleeping on multiple queues or using timeout or interruible sleep, you must
 * call \ref sleep_remove_from queue once the sleep is finished
 * else it's automatic
 * @param queue the queue to add the current task to
 */
void sleep_add_to_queue(sleep_queue_t *queue);

/**
 * @brief remove the current task from a sleep queue
 * @note will not crash if the current task is not in the queue, will just do nothing
 * @param queue the queue to remove the current task from
 */
void sleep_remove_from_queue(sleep_queue_t *queue);

/**
 * @brief sleep on a queue
 * @param queue the queue to sleep on
 * @return same values as \ref block_task
 */
int sleep_on_queue(sleep_queue_t *queue);

/**
 * @brief sleep on a queue and can be interrupted by a signal
 * @param queue the queue to sleep on
 * @return same values as \ref block_task
 */
int sleep_on_queue_interruptible(sleep_queue_t *queue);

/**
 * @brief wakeup a specified number of waiters on a sleep queue
 * @param queue the queue to wakeup waiters on
 * @param count the number of waiters to wakeup or 0 for all
 */
void wakeup_queue(sleep_queue_t *queue, size_t count);

#endif