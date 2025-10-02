#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <kernel/asm.h>
#include <stdatomic.h>

typedef struct spinlock {
    atomic_flag lock;
    int had_interrupt;
} spinlock;

#define SPINLOCK_DEBUG

#ifdef SPINLOCK_DEBUG
#include <kernel/print.h>
#define spinlock_debug(...) kdebugf(__VA_ARGS__);
#else
#define spinlock_debug(...)
#endif
#define spinlock_acquire(l) {while(atomic_flag_test_and_set_explicit(&(l)->lock,memory_order_acquire));(l)->had_interrupt = have_interrupt();disable_interrupt();spinlock_debug("acquire spinlock %s\n",#l)}
#define spinlock_release(l) {atomic_flag_clear_explicit(&(l)->lock,memory_order_release);spinlock_debug("release spinlock %s\n",#l) if((l)->had_interrupt)enable_interrupt();}

#endif