#include <kernel/sleep.h>
#include <kernel/scheduler.h>
#include <kernel/hashmap.h>
#include <kernel/mmu.h>
#include <sys/futex.h>
#include <stdint.h>

static hashmap_t futexes;
static spinlock_t futexes_lock;

static sleep_queue_t *futex_from_addr(long *addr) {
	uintptr_t phys = mmu_virt2phys(addr);
	if (phys == PAGE_INVALID) return NULL;
	return hashmap_get(&futexes, phys);
}

static sleep_queue_t *new_futex_for_addr(long *addr) {
	uintptr_t phys = mmu_virt2phys(addr);
	if (phys == PAGE_INVALID) return NULL;

	// make a new sleep queue
	sleep_queue_t *new_queue = kmalloc(sizeof(sleep_queue_t));
	memset(new_queue, 0, sizeof(sleep_queue_t));

	hashmap_add(&futexes, phys, new_queue);
	return new_queue;
}

static void destroy_futex_for_addr(long *addr) {
	uintptr_t phys = mmu_virt2phys(addr);
	if (phys == PAGE_INVALID) return;
	kfree(hashmap_get(&futexes, phys));
	hashmap_remove(&futexes, phys);
}

static int futex_wake(long *addr, long val) {
	if (val == 0) return 0;

	// futex take LONG_MAX to wakeup all
	// while wakeup_queue take 0 to wakeup all
	if (val == LONG_MAX) val = 0;

	spinlock_acquire(&futexes_lock);

	sleep_queue_t *queue = futex_from_addr(addr);
	if (!queue) {
		spinlock_release(&futexes_lock);
		return 0;
	}
	wakeup_queue(queue, val);
	if (!queue->waiters.first_node) {
		// the futex is empty
		destroy_futex_for_addr(addr);
	}
	spinlock_release(&futexes_lock);
	return 0;
}

static int futex_wait(long *addr, long val) {
	block_prepare();
	if (*addr == val) {
		block_cancel();
		return 0;
	}
	spinlock_acquire(&futexes_lock);
	sleep_queue_t *queue = futex_from_addr(addr);
	if (!queue) {
		queue = new_futex_for_addr(addr);
	}
	if (queue) {
		spinlock_release(&futexes_lock);
		return -EFAULT;
	}

	sleep_add_to_queue(queue);
	spinlock_release(&futexes_lock);
	return block_task();
	// TODO : remove from queue if interrupted
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
	init_hashmap(&futexes, 256);
}
