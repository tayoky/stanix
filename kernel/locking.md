---
title: locking
---
Stanix's kernel provide multiple ways to do locking.

## spinlock
**Spinlocks** (`spinlock_t` in `<kernel/spinlock.h>`) are the most simple locks, they spin until they acquire a lock, they don't yield and are safe to call from any context (including irq handlers).
Spinlocks can be acquired using `spinlock_acquire` and released using `spinlock_release` or `spinlock_raw_release`.
Spinlocks disable interrupts and `spinlock_raw_release` does not restore them while `spinlock_release` does.

## mutexes
**Mutexes** (`mutex_t` in `<kernel/mutex.h>`) are simple lock that can block instead of wasting cpu time.
Mutexes can be acquired using `mutex_acquire` or `mutex_try_acquire` and can be released using `mutex_release`.
`mutex_try_acquire` cannot block and will return 0 if the mutex cannot be acquired.

Some notable behaviour of mutexes include :
- A mutex must be released by the task that acquire it.
- Mutexes cannot be used in irq handlers.
- Mutexes can block.
- A task that already own a mutex can reacquire it incrementing the `lock_depth` (one more `mutex_release` become necessary to release it).

## rwlocks
**Rwlocks** (`rwlock_t` in `<kernel/rwlock.h>`) can have one writer or multiples readers at the same time.
Unlike rwsemaphores, rwlocks do not block and instead spin, they are safe to use in any context (including irq handlers).
A writer lock can be acquired using `rwlock_acquire_write` and released using `rwlock_release_write`.
A reader lock can be acquired using `rwlock_acquire_read` and released using `rwlock_release_read`.
Rwlocks disable interrupts when they are acquired and restore them when they are realased thus, rwlock locks must be released in the reverse order they were acquired.

## rwsems
**Rwsemaphores** (`rwsem_t` in `<kernel/rwsem.h>`) can have one writer or multiples readers at the same time.
A writer lock can be acquired using `rwsem_acquire_write` and released using `rwsem_release_write`.
A reader lock can be acquired using `rwsem_acquire_read` and released using `rwsem_release_read`.

Some notable behaviour of rwsemaphores include :
- A rwsemaphore must be released by the task that acquire it.
- Rwsemaphore cannot be used in irq handlers.
- Rwsemaphore can block.
- A task that aready own a writer lock on a rwsemaphore can reacquire a writer or reader lock incrementing the `lock_depth` (one more `rwsem_release_writer`/`rwsem_release_reader` become necessary to release it).
- A task that aready own a reader lock on a rwsemaphore can reacquire a reader lock but not a writer lock (except if the task also aready own a writer lock, see rule above).

## conditions variables
**Conditions variables** (`cond_t` in `<kernel/cond.h>`) allow to sleep until a variable is set to a specific value using `cond_wait` and `cond_wait_interruptible` the later being interruptible by signals.
The value of a condition variable can be set usinf `cond_set`.
Conditions variables are blocking and are not safe to use from an irq handler except for `cond_set` which does not block but only unblock tasks.

## sleep queues
**Sleep queues** (`sleep_queue_t` in `<kernel/sleep.h>`) are the lowest level syncronisation primitive that can block. It is made only of spinlocks and `block_task`/`unblock_task`.
A task can sleep on a queue using various functions :
- `sleep_on_queue`
  Sleep on a queue.
- `sleep_on_queue_condition`
  Sleep on a queue in a loop until a condition is true atomicly (implemented as a macro so cond get revaluated).
- `sleep_on_queue_lock`
  Same as `sleep_on_queue_condition` but release a spinlock before blocking and acquire it when getting unblocked (the spinlock must be hold when calling this function and the spinlock will stay hold when this function return) (also implemented as a macro).

A task can wakeup a specified amount of task using `wakeup_queue` (an amount of 0 wakeup the whole queue).
