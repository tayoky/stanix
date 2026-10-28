#include <kernel/cache.h>
#include <kernel/scheduler.h>
#include <kernel/sleep.h>

static task_t *writeback_task = NULL;
static task_t *evicter_task   = NULL;

static void writeback_thread() {
	for (;;) {
		size_t clean = cache_get_clean_pages();
		size_t threshold = cache_get_clean_pages_threshold();
		while (clean >= threshold) {
			block_prepare();
			block_task();
			clean = cache_get_clean_pages();
			threshold = cache_get_clean_pages_threshold();
		}
		
		cache_flush_all();
		
		// add a minimum of 5 second between flushs
		nano_sleep(5L * 1000000000L);
		block_prepare();
		block_task();
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

		// evict twice the amount and a minimum of 4 pages
		// to avoid doing really small eviction or failing to evict the specfied pages
		// because of pinned pages
		to_evict *= 2;
		if (to_evict < 4) to_evict = 4;
		
		cache_evict(to_evict);
		block_prepare();
		block_task();
	}
}

void init_fsthreads(void) {
	writeback_task = new_kernel_task(writeback_thread, NULL, "writeback");
	evicter_task   = new_kernel_task(evicter_thread, NULL, "evicter");
}

void fsthreads_wakeup_evicter(void) {
	if (evicter_task) unblock_task(evicter_task);
}
