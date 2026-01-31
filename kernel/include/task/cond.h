#ifndef _KERNEL_COND_H
#define _KERNEL_COND_H

#include <kernel/sleep.h>
#include <stdatomic.h>

typedef struct cond {
	sleep_queue_t queue;
	atomic_int value;
} cond_t;


void init_cond(cond_t *cond);
void free_cond(cond_t *cond);
int cond_wait(cond_t *cond, int value);
int cond_wait_interruptible(cond_t *cond, int value);
void cond_set(cond_t *cond, int value);

#endif