#include <kernel/oneshot.h>
#include <kernel/sleep.h>
#include <kernel/string.h>
#include <errno.h>

void oneshot_init(oneshot_t *oneshot) {
	memset(oneshot, 0, sizeof(oneshot_t));
}

void oneshot_destroy(oneshot_t *oneshot) {
	(void)oneshot;
}

int oneshot_wait(oneshot_t *oneshot) {
	return sleep_on_queue_condition(&oneshot->queue, atomic_load(&oneshot->signaled));
}

int oneshot_wait_interruptible(oneshot_t *oneshot) {
	return sleep_on_queue_condition(&oneshot->queue, atomic_load(&oneshot->signaled));
}

void oneshot_signal(oneshot_t *oneshot) {
	atomic_store(&oneshot->signaled, 1);
	wakeup_queue(&oneshot->queue, 0);
}
