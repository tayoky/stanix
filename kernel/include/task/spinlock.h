#ifndef SPINLOCK_H
#define SPINLOCK_H

// deadlock detection only work on non SMP
#define SPINLOCK_DEBUG

#ifdef SPINLOCK_DEBUG
#include <kernel/macro.h>
#include <kernel/panic.h>
#include <kernel/print.h>
#endif
#include <kernel/asm.h>
#include <stdatomic.h>

typedef struct spinlock {
	atomic_flag lock;
	int had_interrupt;
#ifdef SPINLOCK_DEBUG
	const char *line;
#endif
} spinlock_t;


#ifdef SPINLOCK_DEBUG
static inline void __spinlock_raw_acquire(spinlock_t *lock, const char *line) {
	while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire)) {
		kdebugf("deadlock, lock acquired at %s\n", lock->line);
		panic("deadlock", NULL);
	}
	lock->line = line;
}
static inline void __spinlock_acquire(spinlock_t *lock, const char *line) {
	lock->had_interrupt = have_interrupt();
	disable_interrupt();
    __spinlock_raw_acquire(lock, line);
}
#define spinlock_raw_acquire(lock) __spinlock_raw_acquire(lock, __FILE__ ":" STRINGIFY(__LINE__))
#define spinlock_acquire(lock) __spinlock_acquire(lock, __FILE__ ":" STRINGIFY(__LINE__))
#else
static inline void spinlock_raw_acquire(spinlock_t *lock) {
	while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire));
}

static inline void spinlock_acquire(spinlock_t *lock) {
	lock->had_interrupt = have_interrupt();
	disable_interrupt();
	spinlock_raw_acquire(lock);
}
#endif

static inline void spinlock_raw_release(spinlock_t *lock) {
	atomic_flag_clear_explicit(&lock->lock, memory_order_release);
}

static inline void spinlock_release(spinlock_t *lock) {
	spinlock_raw_release(lock);
	if (lock->had_interrupt) enable_interrupt();
}

#endif
