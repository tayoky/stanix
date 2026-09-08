#include <kernel/hashmap.h>
#include <kernel/mmu.h>
#include <kernel/scheduler.h>
#include <kernel/sleep.h>
#include <sys/futex.h>
#include <errno.h>
#include <stdint.h>

static sleep_queue_t futexes[128];

static sleep_queue_t *futex_from_addr(long *addr) {
	uintptr_t phys = mmu_virt2phys(addr);
	if (phys == PAGE_INVALID) return NULL;

	// TODO : use a hash
	return &futexes[phys % arraylen(futexes)];
}

static int futex_wake(long *addr, long val) {
	if (val == 0) return 0;

	// futex take LONG_MAX to wakeup all
	// while wakeup_queue take 0 to wakeup all
	if (val == LONG_MAX) val = 0;

	sleep_queue_t *queue = futex_from_addr(addr);
	if (!queue) return -EFAULT;

	wakeup_queue(queue, val);
	return 0;
}

static int futex_wait(long *addr, long val) {
	sleep_queue_t *queue = futex_from_addr(addr);
	if (!queue) return -EFAULT;

	block_prepare_interruptible();
	if (*addr != val) {
		block_cancel();
		return -EAGAIN;
	}

	sleep_add_to_queue(queue);
	if (*addr != val) {
		sleep_remove_from_queue(queue);
		block_cancel();
		return -EAGAIN;
	}
	int ret = block_task();
	if (ret == -EINTR) {
		sleep_remove_from_queue(queue);
	}
	return ret;
}

int do_futex(long *addr, int op, long val) {
	switch (op) {
	case FUTEX_WAKE:
		return futex_wake(addr, val);
	case FUTEX_WAIT:
		return futex_wait(addr, val);
	default:
		return -EINVAL;
	}
}

void init_futexes(void) {
}
