#ifndef KERNEL_REFCOUNT_H
#define KERNEL_REFCOUNT_H

#include <stdatomic.h>
#include <stddef.h>

typedef atomic_size_t ref_count_t;

/**
 * @brief increment a ref count
 * @param ref_count the ref count to increment
 */
static inline void ref_count_inc(ref_count_t *ref_count) {
	atomic_fetch_add(ref_count, 1);
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
