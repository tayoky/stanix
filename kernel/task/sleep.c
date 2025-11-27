#include <kernel/time.h>
#include <kernel/sleep.h>
#include <kernel/kernel.h>
#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/print.h>
#include <errno.h>

int sleep_until(struct timeval wakeup_time) {
	kdebugf("wait until : %ld:%ld\n", wakeup_time.tv_sec, wakeup_time.tv_usec);
	get_current_task()->wakeup_time = wakeup_time;
	atomic_fetch_or(&get_current_task()->flags, TASK_FLAG_SLEEP);

	//add us to the list
	//keep the list organise from first awake to last
	spinlock_acquire(&sleep_lock);
	task_t *thread = sleeping_proc;
	while (thread) {
		task_t *next = thread->snext;
		if (!next || next->wakeup_time.tv_sec > wakeup_time.tv_usec || (next->wakeup_time.tv_sec == wakeup_time.tv_sec && next->wakeup_time.tv_usec > wakeup_time.tv_usec)) {
			break;
		}
		thread = thread->snext;
	}

	if (thread) {
		get_current_task()->snext = thread->snext;
		get_current_task()->sprev = thread;
		if (thread->snext) thread->snext->sprev = get_current_task();
		thread->snext   = get_current_task();
	} else {
		get_current_task()->snext = sleeping_proc;
		get_current_task()->sprev = NULL;
		if (sleeping_proc) sleeping_proc->sprev = get_current_task();
		sleeping_proc = get_current_task();
	}
	spinlock_release(&sleep_lock);

	if (block_task() == -EINTR) {
		if (atomic_load(&get_current_task()->flags) & TASK_FLAG_SLEEP) {
			// we are still in the sleep queue
			// remove us
			spinlock_acquire(&sleep_lock);
			if (get_current_task()->sprev) {
				get_current_task()->sprev->snext = get_current_task()->snext;
			} else {
				sleeping_proc = get_current_task()->snext;
			}
			if (get_current_task()->snext) {
				get_current_task()->snext->sprev = get_current_task()->sprev;
			}
			spinlock_release(&sleep_lock);
		}
		atomic_fetch_and(&get_current_task()->flags, ~TASK_FLAG_SLEEP);
		return -EINTR;
	} else {
		return 0;
	}
}

int sleep(long seconds) {
	return micro_sleep(seconds * 1000000);
}

int micro_sleep(suseconds_t micro_second) {
	if (micro_second < 100) {
		return 0;
	}
	//caclulate new time val
	struct timeval new_timeval = time;
	if (1000000 - new_timeval.tv_usec > micro_second) {
		new_timeval.tv_usec += micro_second;
	} else {
		micro_second -= 1000000 - new_timeval.tv_usec;
		new_timeval.tv_sec++;
		new_timeval.tv_usec = 0;

		//now we need to cut the remaing micro second in second and micro second
		new_timeval.tv_sec += micro_second / 1000000;
		new_timeval.tv_usec = micro_second % 1000000;
	}

	return sleep_until(new_timeval);
}

int sleep_on_queue(sleep_queue *queue) {
	spinlock_acquire(&queue->lock);

	if (queue->head) {
		queue->head->snext = get_current_task();
	} else {
		queue->tail = get_current_task();
	}

	queue->head = get_current_task();
	get_current_task()->snext = NULL;

	spinlock_release(&queue->lock);

	//what if we get unblocked between releasing the lock and block_task
	//RACE CONDITION

	return block_task();
}

void wakeup_queue(sleep_queue *queue, size_t count) {
	spinlock_acquire(&queue->lock);

	for (;;) {
		if (!queue->tail)break;

		task_t *thread = queue->tail;

		queue->tail = queue->tail->snext;
		if (!queue->tail)queue->head = NULL;

		unblock_task(thread);

		if (count) {
			if (--count == 0)break;
		}
	}

	spinlock_release(&queue->lock);
}