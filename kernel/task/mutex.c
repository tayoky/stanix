#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/mutex.h>

void init_mutex(mutex_t *mutex){
	memset(mutex,0,sizeof(mutex_t));
}

int acquire_mutex(mutex_t *mutex){
	spinlock_acquire(mutex->lock);
	if(mutex->locked){
		spinlock_release(mutex->lock);
		list_append(&mutex->waiter,get_current_proc());
		block_proc();
		spinlock_acquire(mutex->lock);
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
	if(mutex->waiter.node_count > 0){
		unblock_proc(mutex->waiter.frist_node->value);
		list_remove(&mutex->waiter,mutex->waiter.frist_node->value);
	}
	spinlock_release(mutex->lock);
}