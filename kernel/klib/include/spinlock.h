#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdatomic.h>

typedef atomic_flag spinlock;

#define spinlock_acquire(lock) while(atomic_flag_test_and_set_explicit(lock,memory_order_acquire))
#define spinlock_release(lock) atomic_flag_clear_explicit(lock,memory_order_release)

#endif