#ifndef KERNEL_RCU_H
#define KERNEL_RCU_H

#include <kernel/atomic.h>
#include <kernel/scheduler.h>
#include <kernel/spinlock.h>

typedef ATOMIC(void *) rcu_ptr_t;

/**
 * @brief acquire the rcu read lock
 */
static inline void rcu_acquire_read(void) {
	preempt_disable();
}

/**
 * @brief release the rcu read lock
 */
static inline void rcu_release_read(void) {
	preempt_enable();
}

/**
 * @brief get atomicly fetch a rcu pointer
 * @param ptr the pointer to fetch
 * @return the value of the pointer
 */
static inline void *rcu_ptr_fetch(rcu_ptr_t *ptr) {
	return atomic_load(ptr);
}

/**
 * @brief set a rcu pointer
 * @param ptr the rcu pointer to set
 * @param value the value of the new ptr
 * @return the old value of the pointer
 */
static inline void *rcu_ptr_store(rcu_ptr_t *ptr, void *value) {
	return atomic_exchange(ptr, value);
}

/**
 * @brief wait until the end of the rcu grace period
 */
static inline void rcu_sync(void) {
	// TODO : complete this when we get SMP
}

#endif
