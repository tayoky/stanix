#include <kernel/condvar.h>
#include <kernel/sleep.h>

#define QUEUES_COUNT 64

static sleep_queue_t queues[QUEUES_COUNT];

static sleep_queue_t *get_queue(cond_var_t *var) {
	// TODO : true hash
	size_t hash = ((uintptr_t)var / sizeof(cond_var_t));
	return &queues[hash % QUEUES_COUNT];
}

int cond_var_wait(cond_var_t *var, unsigned int mask, unsigned int value) {
	return sleep_on_queue_condition(get_queue(var), (atomic_load(var) & mask) == value);
}

int cond_var_wait_interruptible(cond_var_t *var, unsigned int mask, unsigned int value) {
	return sleep_on_queue_condition(get_queue(var), (atomic_load(var) & mask) == value);
}

void cond_var_wakeup(cond_var_t *var) {
	wakeup_queue(get_queue(var), 0);
}
