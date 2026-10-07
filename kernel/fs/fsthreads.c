#include <kernel/cache.h>
#include <kernel/scheduler.h>
#include <kernel/sleep.h>

static task_t *writeback_task = NULL;
static task_t *evicter_task   = NULL;

static void writeback_thread() {
	for (;;) {
		nano_sleep(30L * 1000000000L);
		cache_flush_all();
	}
}

static void evicter_thread() {
	for (;;) {
		size_t free = pmm_get_free_pages();
		size_t threshold = pmm_get_free_pages_threshold();
		while (free >= threshold) {
			block_prepare();
			block_task();
			free = pmm_get_free_pages();
			threshold = pmm_get_free_pages_threshold();
		}
		size_t to_evict = threshold - free;
		cache_evict(to_evict);
		block_prepare();
		block_task();
	}
}

void init_fsthreads(void) {
	writeback_task = new_kernel_task(writeback_thread, NULL);
	evicter_task   = new_kernel_task(evicter_thread, NULL);
}

void fsthreads_wakeup_evicter(void) {
	if (evicter_task) unblock_task(evicter_task);
}
