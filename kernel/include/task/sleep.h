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
		sleep_add_to_queue(queue);\
		if (cond) {\
			block_cancel();\
			break;\
		}\
		\
		if (l) spinlock_raw_release(l);\
		\
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
		sleep_add_to_queue(queue);\
		if (cond) {\
			block_cancel();\
			break;\
		}\
		\
		if (l) spinlock_raw_release(l);\
		\
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
 * call \ref sleep_remove_from_queue once the sleep is finished
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
 * @brief add the current task to the timeout queue
 * @note if sleeping on multiple queues or using timeout or interruible sleep, you must
 * call \ref sleep_remove_timeout queue once the sleep is finished
 * else it's automatic
 * @param wakeup_time wakeup when time reach this timeval
 */
void sleep_add_timeout(struct timeval *wakeup_time);

/**
 * @brief remove the current task from the timeout queue
 */
void sleep_remove_timeout(void);

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

/**
 * @brief sleep until a timeval
 * @param wakeup_time the timeval to sleep until
 * @return 0 on success or -EINTR if interrupted
 */
int sleep_until(struct timeval wakeup_time);

/**
 * @brief sleep for the specified time
 * @param second the time to sleep for, in seconds
 * @return 0 on success or -EINTR if interrupted
 */
int sleep(long second);

/**
 * @brief sleep for the specified time
 * @param micro_second the time to sleep for, in micro seconds
 * @return 0 on success or -EINTR if interrupted
 */
int micro_sleep(suseconds_t micro_second);


#endif