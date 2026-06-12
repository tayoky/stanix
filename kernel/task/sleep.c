#include <kernel/time.h>
#include <kernel/sleep.h>
#include <kernel/kernel.h>
#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/print.h>
#include <kernel/slab.h>
#include <errno.h>

static slab_cache_t sleep_nodes_slab;

void init_sleep(void) {
	slab_init(&sleep_nodes_slab, sizeof(sleep_queue_node_t), "sleep-queue-nodes");
}

void sleep_add_timeout(struct timespec *wakeup_time) {
	get_current_task()->wakeup_time = *wakeup_time;
	atomic_fetch_or(&get_current_task()->flags, TASK_FLAG_SLEEP);
	
	// add us to the list
	// keep the list organised from first awake to last
	spinlock_acquire(&sleep_lock);
	task_t *prev = NULL;
	foreach (node, &sleeping_tasks) {
		task_t *task = container_of(node, task_t, waiter_list_node);
		if (timespec_cmp(wakeup_time, &task->wakeup_time) < 0) {
			break;
		}
		prev = task;
	}

	list_add_after(&sleeping_tasks, prev ? &prev->waiter_list_node : NULL, &get_current_task()->waiter_list_node);
	spinlock_release(&sleep_lock);
}

void sleep_remove_timeout(void) {
	spinlock_acquire(&sleep_lock);
	if (atomic_fetch_and(&get_current_task()->flags, ~TASK_FLAG_SLEEP) & TASK_FLAG_SLEEP) {
		// we are still in the sleep queue
		// remove us
		list_remove(&sleeping_tasks, &get_current_task()->waiter_list_node);
	}
	spinlock_release(&sleep_lock);
}

int sleep_until(struct timespec *wakeup_time) {
	block_prepare_interruptible();
	int ret = block_task_timeout(wakeup_time);
	if (ret == -ETIMEDOUT) ret = 0;
	return ret;
}

int sleep(long seconds) {
	return micro_sleep(seconds * 1000000);
}

int nano_sleep(long nano_seconds) {
	if (nano_seconds < 100) {
		return 0;
	}
	// caclulate new timespec
	struct timespec new_timespec;
	gettime(CLOCK_MONOTONIC, &new_timespec);
	if (1000000000 - new_timespec.tv_nsec > nano_seconds) {
		new_timespec.tv_nsec += nano_seconds;
	} else {
		nano_seconds -= 1000000000 - new_timespec.tv_nsec;
		new_timespec.tv_sec++;
		new_timespec.tv_nsec = 0;

		//now we need to cut the remaing micro second in second and micro second
		new_timespec.tv_sec += nano_seconds / 1000000;
		new_timespec.tv_nsec = nano_seconds % 1000000;
	}

	return sleep_until(&new_timespec);
}


void sleep_add_to_queue_unlocked(sleep_queue_t *queue) {
	sleep_queue_node_t *node = slab_alloc(&sleep_nodes_slab);
	node->task = get_current_task();
	list_append(&queue->waiters, &node->node);
}

void sleep_add_to_queue(sleep_queue_t *queue) {
	spinlock_acquire(&queue->lock);
	sleep_add_to_queue_unlocked(queue);
	spinlock_release(&queue->lock);
}

void sleep_remove_from_queue(sleep_queue_t *queue) {
	spinlock_acquire(&queue->lock);
	foreach (current, &queue->waiters) {
		sleep_queue_node_t *node = container_of(current, sleep_queue_node_t, node);
		if (node->task != get_current_task()) continue;
		list_remove(&queue->waiters, &node->node);
		slab_free(node);
		break;
	}
	spinlock_release(&queue->lock);
}

int sleep_on_queue(sleep_queue_t *queue) {
	block_prepare();

	sleep_add_to_queue(queue);
	
	return block_task();
}

int sleep_on_queue_interruptible(sleep_queue_t *queue) {
	block_prepare_interruptible();

	sleep_add_to_queue(queue);

	int ret = block_task();
	if (ret < 0) {
		// if interrupted must remove manually
		sleep_remove_from_queue(queue);
	}
	return ret;
}


void wakeup_queue(sleep_queue_t *queue, size_t count) {
	spinlock_acquire(&queue->lock);

	list_node_t *current = queue->waiters.first_node;
	while (current) {
		sleep_queue_node_t *node = container_of(current, sleep_queue_node_t, node);
		current = current->next;
		list_remove(&queue->waiters, &node->node);
		unblock_task(node->task);
		slab_free(node);

		if (count) {
			if (--count == 0)break;
		}
	}

	spinlock_release(&queue->lock);
}
