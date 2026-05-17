#ifndef KERNEL_RCU_H
#define KERNEL_RCU_H

#include <stdatomic.h>
#include <kernel/spinlock.h>
#include <kernel/scheduler.h>

typedef atomic_uintptr_t rcu_ptr_t;

typedef struct rcu {
    rcu_ptr_t ptr;
    spinlock_t lock;
} rcu_t;

/**
 * @brief acquire the read lock of a rcu
 * @param rcu the rcu to acquire the read lock of
 */
static inline void rcu_acquire_read(rcu_t *rcu) {
    (void)rcu;
    preempt_disable();
}

/**
 * @brief release the read lock of a rcu
 * @param rcu the rcu to release the read lock of
 */
static inline void rcu_release_read(rcu_t *rcu) {
    (void)rcu;
    preempt_enable();
}

/**
 * @brief get atomicly fetch a rcu pointer
 * @param ptr the pointer to fetch
 * @return the value of the pointer
 */
static inline void *rcu_ptr_fetch(rcu_ptr_t *ptr) {
    return (void*)atomic_load(ptr);
}

/**
 * @brief get the pointer of a rcu, require the read lock or the write lock to be acquired
 * @param rcu the rcu to get the pointer of
 * @return the pointer of the rcu
 */
static inline void *rcu_fetch_ptr(rcu_t *rcu) {
    return rcu_ptr_fetch(&rcu->ptr);
}


/**
 * @brief acquire the write lock of a rcu
 * @param rcu the rcu to acquire the write lock of
 */
static inline rcu_raw_acquire_write(rcu_t *rcu) {
    spinlock_raw_acquire(&rcu->lock);
}

/**
 * @brief release the write lock of a rcu
 * @param rcu the rcu to release the write lock of
 */
static inline rcu_raw_release_write(rcu_t *rcu) {
    spinlock_raw_release(&rcu->lock);
}

/**
 * @brief acquire the write lock of a rcu and disable interruptions
 * @param rcu the rcu to acquire the write lock of
 */
static inline rcu_acquire_write(rcu_t *rcu) {
    spinlock_acquire(&rcu->lock);
}

/**
 * @brief release the write lock of a rcu and restore interruptions
 * @param rcu the rcu to release the write lock of
 */
static inline rcu_release_write(rcu_t *rcu) {
    spinlock_release(&rcu->lock);
}

/**
 * @brief set a rcu pointer
 * @param ptr the rcu pointer to set
 * @param value the value of the new ptr
 * @return the old value of the pointer
 */
static inline void *rcu_ptr_store(rcu_ptr_t *ptr, void *value) {
    return atomic_exchange(ptr, (uintptr_t)value);
}

/**
 * @brief set the pointer of a rcu, require the write lock to be acquired
 * @param rcu the rcu to set the pointer of
 * @param value the new pointer value
 * @return the old pointer value
 */
static inline void *rcu_store_ptr(rcu_t *rcu, void *value) {
    return rcu_ptr_store(&rcu->ptr, value);
}

/**
 * @brief wait until the grace period of rcu as ended
 * @param rcu the rcu to wait the grace period for
 */
static inline void rcu_sync(rcu_t *rcu) {
    (void)rcu;
    // TODO : complete this when we get SMP
}

#endif
