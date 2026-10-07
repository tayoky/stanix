#include <kernel/cache.h>
#include <kernel/scheduler.h>
#include <kernel/sleep.h>

static task_t *writeback_task;
static task_t *evicter_task;

static void writeback_thread() {
    for (;;) {
        nano_sleep(30L * 1000000000L);
        cache_flush_all();
    }
}

static void evicter_thread() {
    for (;;) {
        size_t target = pmm_get_total_pages() - pmm_get_emergency_pages();
        size_t used = pmm_get_used_pages();
        while (used < target) {
            block_prepare();
            block_task();
        }
        size_t to_evict = used - target;
        cache_evict(to_evict);
        block_prepare();
        block_task();
    }
}

void init_fsthreads(void) {
    writeback_task = new_kernel_task(writeback_thread, NULL);
    evicter_task = new_kernel_task(evicter_thread, NULL);
}

void fsthreads_wakeup_evicter(void) {
    unblock_task(evicter_task);
}
