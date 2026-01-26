#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <kernel/asm.h>
#include <stdatomic.h>

typedef struct spinlock {
    atomic_flag lock;
    int had_interrupt;
} spinlock_t;


static inline void spinlock_acquire(spinlock_t *lock) {
    while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire));
    lock->had_interrupt = have_interrupt();
    disable_interrupt();
}

static inline void spinlock_release(spinlock_t *lock) {
    atomic_flag_clear_explicit(&lock->lock, memory_order_release);
    if (lock->had_interrupt) enable_interrupt();
}

#endif