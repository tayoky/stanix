#ifndef KERNEL_ONESHOT_H
#define KERNEL_ONESHOT_H

#include <kernel/sleep.h>
#include <kernel/atomic.h>

typedef struct oneshot {
	sleep_queue_t queue;
	ATOMIC(int) signaled;;
} oneshot_t;

void oneshot_init(oneshot_t *oneshot);
void oneshot_destroy(oneshot_t *oneshot);
int oneshot_wait(oneshot_t *oneshot);
int oneshot_wait_interruptible(oneshot_t *oneshot);
void oneshot_signal(oneshot_t *oneshot);

#endif
