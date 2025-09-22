#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <kernel/asm.h>
#include <stdatomic.h>

typedef struct spinlock {
    atomic_flag lock;
    int had_interrupt;
} spinlock;

#define spinlock_acquire(l) {while(atomic_flag_test_and_set_explicit(&(l)->lock,memory_order_acquire));(l)->had_interrupt = have_interrupt();disable_interrupt();}
#define spinlock_release(l) {atomic_flag_clear_explicit(&(l)->lock,memory_order_release); if((l)->had_interrupt)enable_interrupt();}

#endif