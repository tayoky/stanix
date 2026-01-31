#include <kernel/cond.h>
#include <kernel/sleep.h>
#include <kernel/string.h>

void init_cond(cond_t *cond) {
	memset(cond, 0, sizeof(cond_t));
}

void free_cond(cond_t *cond) {
	(void)cond;
}

int cond_wait(cond_t *cond, int value) {
	while (sleep_on_queue_condition(&cond->queue, atomic_load(&cond->value) == value) == -EINTR);
	return 0;
}

int cond_wait_interruptible(cond_t *cond, int value) {
	return sleep_on_queue_condition(&cond->queue, atomic_load(&cond->value) == value);
}

void cond_set(cond_t *cond, int value) {
	atomic_store(&cond->value, value);
	wakeup_queue(&cond->queue, 0);
}