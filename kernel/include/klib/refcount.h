#ifndef KERNEL_REFCOUNT_H
#define KERNEL_REFCOUNT_H

#include <kernel/atomic.h>
#include <stddef.h>

typedef ATOMIC(size_t) ref_count_t;

/**
 * @brief increment a ref count
 * @param ref_count the ref count to increment
 */
static inline void ref_count_inc(ref_count_t *ref_count) {
	atomic_fetch_add(ref_count, 1);
}

/**
 * @brief increment a ref count if not zero
 * @param ref_count the ref count to increment
 * @return 1 if incremented else 0
 */
static inline int ref_count_inc_if_not_zero(ref_count_t *ref_count) {
	size_t old = atomic_load(ref_count);
	while (old != 0) {
		if (atomic_compare_exchange_weak(ref_count, &old, old + 1)) {
			return 1;
		}
		// we raced and need to retry
	}
	return 0;
}

/**
 * @brief decrement a ref count
 * @param ref_count the ref count to decrement
 * @return the previous value of the ref count
 */
static inline size_t ref_count_dec(ref_count_t *ref_count) {
	return atomic_fetch_sub(ref_count, 1);
}

#endif
