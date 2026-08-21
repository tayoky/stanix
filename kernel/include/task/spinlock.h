#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

// deadlock detection only work on non SMP
#define SPINLOCK_DEBUG

#ifdef SPINLOCK_DEBUG
#include <kernel/macro.h>
#include <kernel/panic.h>
#include <kernel/print.h>
#endif
#include <kernel/asm.h>
#include <kernel/assert.h>
#include <stdatomic.h>

typedef struct spinlock {
	atomic_flag lock;
#ifdef SPINLOCK_DEBUG
	const char *line;
#endif
} spinlock_t;

void preempt_enable(void);
void preempt_disable(void);
	
#ifdef SPINLOCK_DEBUG
static inline void __spinlock_raw_acquire(spinlock_t *lock, const char *line) {
	while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire)) {
		kdebugf("deadlock on %s, lock acquired at %s\n", line, lock->line);
		panic("deadlock", NULL);
	}
	lock->line = line;
}

static inline void __spinlock_acquire(spinlock_t *lock, const char *line) {
	preempt_disable();
    __spinlock_raw_acquire(lock, line);
}

static inline int __spinlock_acquire_irq(spinlock_t *lock, const char *line) {
	int irq_save = have_interrupt();
	disable_interrupt();
    __spinlock_raw_acquire(lock, line);
	return irq_save;
}

#define spinlock_raw_acquire(lock) __spinlock_raw_acquire(lock, __FILE__ ":" STRINGIFY(__LINE__))
#define spinlock_acquire(lock) __spinlock_acquire(lock, __FILE__ ":" STRINGIFY(__LINE__))
#define spinlock_acquire_irq(lock) __spinlock_acquire_irq(lock, __FILE__ ":" STRINGIFY(__LINE__))
#else
static inline void spinlock_raw_acquire(spinlock_t *lock) {
	while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire));
}

static inline void spinlock_acquire(spinlock_t *lock) {
	preempt_disable();
	spinlock_raw_acquire(lock);
}

static inline int spinlock_acquire_irq(spinlock_t *lock) {
	int irq_save = have_interrupt();
	disable_interrupt();
	spinlock_raw_acquire(lock);
	return irq_save;
}
#endif

static inline void spinlock_raw_release(spinlock_t *lock) {
	atomic_flag_clear_explicit(&lock->lock, memory_order_release);
}

static inline void spinlock_release(spinlock_t *lock) {
	spinlock_raw_release(lock);
	preempt_enable();
}

static inline void spinlock_release_irq(spinlock_t *lock, int irq_save) {
	spinlock_raw_release(lock);
	if (irq_save) enable_interrupt();
}

#define spinlock_assert_acquired(_lock) kassert(atomic_flag_test_and_set(&(_lock)->lock) && "spinlock acquired")

#endif
