#ifndef _KERNEL_RWLOCK_H
#define _KERNEL_RWLOCK_H

#include <kernel/spinlock.h>
#include <stdatomic.h>
#include <limits.h>

typedef struct rwlock {
	atomic_size_t lock;
} rwlock_t;

static inline int rwlock_raw_try_acquire_write(rwlock_t *rwlock) {
	size_t expected = 0;
	return atomic_compare_exchange_strong_explicit(&rwlock->lock, &expected, SIZE_MAX, memory_order_acquire, memory_order_relaxed);
}

static inline void rwlock_raw_acquire_write(rwlock_t *rwlock) {
	while (!rwlock_raw_try_acquire_write(rwlock));
}

static inline void rwlock_raw_release_write(rwlock_t *rwlock) {
	atomic_store_explicit(&rwlock->lock, 0, memory_order_release);
}

static inline int rwlock_raw_try_acquire_read(rwlock_t *rwlock) {
	size_t old = atomic_load_explicit(&rwlock->lock, memory_order_acquire);
	size_t new;

	// retry until we don't race
	do {
		if (old == SIZE_MAX) return 0;
		new = old + 1;
	} while (!atomic_compare_exchange_weak_explicit(&rwlock->lock, &old, new, memory_order_acquire, memory_order_acquire));
	return 1;
}

static inline void rwlock_raw_acquire_read(rwlock_t *rwlock) {
	while (!rwlock_raw_try_acquire_read(rwlock));
}

static inline void rwlock_raw_release_read(rwlock_t *rwlock) {
	atomic_fetch_sub_explicit(&rwlock->lock, 1, memory_order_release);
}

#define __WRAPPERS(name) static inline int rwlock_try_acquire_ ## name(rwlock_t *rwlock, int *interrupt_save) {\
	if (interrupt_save) {\
		*interrupt_save = have_interrupt();\
		disable_interrupt();\
	}\
	int ret = rwlock_raw_try_acquire_ ## name(rwlock);\
	if(ret == 0 && interrupt_save && *interrupt_save) {\
		enable_interrupt();\
	}\
	return ret;\
}\
\
static inline void rwlock_acquire_ ## name(rwlock_t *rwlock, int *interrupt_save) {\
	if (interrupt_save) {\
		*interrupt_save = have_interrupt();\
		disable_interrupt();\
	}\
	rwlock_raw_acquire_ ## name(rwlock);\
}\
\
static inline void rwlock_release_ ## name(rwlock_t *rwlock, int *interrupt_save) {\
	rwlock_raw_release_ ## name(rwlock);\
	if (interrupt_save && *interrupt_save) enable_interrupt();\
}

__WRAPPERS(write)
__WRAPPERS(read)
#undef __WRAPPERS
#endif
