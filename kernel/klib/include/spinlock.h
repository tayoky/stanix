#ifndef SPINLOCK_H
#define SPINLOCK_H

#define spinlock_acquire(lock) while(lock);lock = 1
#define spinlock_release(lock) lock = 0

#endif