#ifndef _KERNEL_RWSEM_H
#define _KERNEL_RWSEM_H

#include <kernel/spinlock.h>
#include <kernel/scheduler.h>
#include <kernel/sleep.h>

/**
 * @brief represent a read-write lock
 * @note you can acqurie another write/read inside a write but not a write inside a read
 */
typedef struct rwsem {
	spinlock_t lock;
	sleep_queue_t writer_queue;
	sleep_queue_t reader_queue;
	size_t waiting_writers_count;
	size_t readers_count;
	size_t lock_depth;
	task_t *writer_active;
} rwsem_t;

void init_rwsem(rwsem_t *rwsem);
int rwsem_acquire_read(rwsem_t *rwsem);
int rwsem_release_read(rwsem_t *rwsem);
int rwsem_acquire_write(rwsem_t *rwsem);
int rwsem_release_write(rwsem_t *rwsem);

#endif
