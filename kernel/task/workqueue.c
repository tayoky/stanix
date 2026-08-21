#include <kernel/workqueue.h>
#include <kernel/scheduler.h>

// TODO : create multiple queues and workers

static list_t works;
static spinlock_t works_lock;
static task_t *worker;

static work_t *get_work(void) {
	int irq_save = spinlock_acquire_irq(&works_lock);
	block_prepare();
	while (list_is_empty(&works)) {
		spinlock_release_irq(&works_lock, irq_save);
		block_task();
		irq_save = spinlock_acquire_irq(&works_lock);
		block_prepare();
	}
	block_cancel();
	work_t *work = container_of(works.first_node, work_t, node);
	list_remove(&works, &work->node);
	work->is_queued = 0;
	spinlock_release_irq(&works_lock, irq_save);
	return work;
}

static void worker_thread() {
	for (;;) {
		work_t *work = get_work();
		work->callback(work);
	}
}

void init_workqueue(void) {
	list_init(&works);
	worker = new_kernel_task(worker_thread, NULL);
}

static void work_raw_queue(work_t *work) {
	if (work->is_queued) return;
	work->is_queued = 1;
	list_append(&works, &work->node);
}

void work_queue(work_t *work) {
	int irq_save = spinlock_acquire_irq(&works_lock);
	work_raw_queue(work);
	spinlock_release_irq(&works_lock, irq_save);
	unblock_task(worker);
}
