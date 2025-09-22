#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/mutex.h>
#include <kernel/panic.h>
#include <stdatomic.h>

//FIXME : should we disable interrupt on mutex

void init_mutex(mutex_t *mutex){
	memset(mutex,0,sizeof(mutex_t));
}

int acquire_mutex(mutex_t *mutex){
	if(atomic_exchange(&mutex->locked,1)){
		//register on the list
		spinlock_acquire(&mutex->lock);
		if(mutex->waiter_head){
			mutex->waiter_head->snext = get_current_proc();
		}
		get_current_proc()->snext = NULL;
		mutex->waiter_head = get_current_proc();
		if(!mutex->waiter_tail)mutex->waiter_tail = mutex->waiter_head;
		mutex->waiter_count++;
		spinlock_release(&mutex->lock);
		while(mutex->locked){
			//if we get intterupted just reblock
			block_proc(NULL);
		}
	}
	return 0;
}

void release_mutex(mutex_t *mutex){
	atomic_store(&mutex->locked,0); //maybee move this at the end
	spinlock_acquire(&mutex->lock);
	if(mutex->waiter_count > 0){
		process *proc = mutex->waiter_tail;
		mutex->waiter_tail = proc->snext;
		if(!mutex->waiter_tail)mutex->waiter_head = NULL;
		mutex->waiter_count--;
		unblock_proc(proc);
	}
	spinlock_release(&mutex->lock);
}

int try_acquire_mutex(mutex_t *mutex){
	return !!atomic_exchange(&mutex->locked,1);
}