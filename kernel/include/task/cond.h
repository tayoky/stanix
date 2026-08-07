#ifndef KERNEL_COND_H
#define KERNEL_COND_H

#include <kernel/sleep.h>
#include <stdatomic.h>

typedef struct cond {
	sleep_queue_t queue;
	atomic_int value;
} cond_t;


void cond_init(cond_t *cond);
void cond_destroy(cond_t *cond);
int cond_wait(cond_t *cond, int value);
int cond_wait_interruptible(cond_t *cond, int value);
void cond_set(cond_t *cond, int value);

#endif