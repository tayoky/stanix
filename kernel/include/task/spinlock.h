#ifndef SPINLOCK_H
#define SPINLOCK_H

//deadlock detection only work on non SMP
//#define SPINLOCK_DEBUG

#ifdef SPINLOCK_DEBUG
#include <kernel/panic.h>
#include <kernel/macro.h>
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
static inline void __spinlock_acquire(spinlock_t *lock, const char *line) {
    lock->had_interrupt = have_interrupt();
    disable_interrupt();
    while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire)){
        kdebugf("deadlock, lock acquired at %s\n", lock->line);
        panic("deadlock", NULL);
    }
    lock->line = line;
}
#define spinlock_acquire(lock) __spinlock_acquire(lock, __FILE__ ":" STRINGIFY(__LINE__))
#else
static inline void spinlock_acquire(spinlock_t *lock) {
    lock->had_interrupt = have_interrupt();
    disable_interrupt();
    while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire));
}
#endif

static inline void spinlock_release(spinlock_t *lock) {
    atomic_flag_clear_explicit(&lock->lock, memory_order_release);
    if (lock->had_interrupt) enable_interrupt();
}

static inline void spinlock_raw_release(spinlock_t *lock) {
    atomic_flag_clear_explicit(&lock->lock, memory_order_release);
}

#endif