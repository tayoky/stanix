#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/mutex.h>
#include <kernel/panic.h>

void init_mutex(mutex_t *mutex){
	memset(mutex,0,sizeof(mutex_t));
}

int acquire_mutex(mutex_t *mutex){
	spinlock_acquire(mutex->lock);
	if(mutex->locked){
		if(mutex->waiter_count >= 32){
			//we can't wait
			//that mean there 32 waiter just trigger a panic at this point
			panic("too much waiter on mutex",NULL);
		}
		//register on waiter list and wait
		mutex->waiter[mutex->waiter_count] = get_current_proc();
		mutex->waiter_count++;
		while(mutex->locked){
			//if wwe get intterupted just reblock
			spinlock_release(mutex->lock);
			block_proc();
			spinlock_acquire(mutex->lock);
		}
	}
	mutex->locked = 1;
	spinlock_release(mutex->lock);
	return 0;
}

int try_acquire_mutex(mutex_t *mutex){
	spinlock_acquire(mutex->lock);
	if(mutex->locked){
		spinlock_release(mutex->lock);
		return 0;
	}
	mutex->locked = 1;
	spinlock_release(mutex->lock);
	return 1;
}

void release_mutex(mutex_t *mutex){
	spinlock_acquire(mutex->lock);
	mutex->locked = 0;

	//this mutex use a LIFO which mean that if there multiples waiter the last to acquire get the mutex
	//this is very bad
	if(mutex->waiter_count > 0){
		mutex->waiter_count--;
		unblock_proc(mutex->waiter[mutex->waiter_count]);
	}
	spinlock_release(mutex->lock);
}