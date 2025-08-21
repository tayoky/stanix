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
		//register on the list
		if(mutex->waiter_head){
			mutex->waiter_head->snext = get_current_proc();
		}
		get_current_proc()->snext = NULL;
		mutex->waiter_head = get_current_proc();
		if(!mutex->waiter_tail)mutex->waiter_tail = mutex->waiter_head;
		mutex->waiter_count++;
		while(mutex->locked){
			//if we get intterupted just reblock
			spinlock_release(mutex->lock);
			block_proc(); //TODO : maybee block_proc should realse the spinlock ?
			spinlock_acquire(mutex->lock);
		}
	}
	mutex->locked = 1;
	spinlock_release(mutex->lock);
	return 0;
}

//TODO : maybee don't unlock on  wakeup ?
void release_mutex(mutex_t *mutex){
	spinlock_acquire(mutex->lock);
	mutex->locked = 0;

	if(mutex->waiter_count > 0){
		process *proc = mutex->waiter_tail;
		mutex->waiter_tail = proc->snext;
		if(!mutex->waiter_tail)mutex->waiter_head = NULL;
		mutex->waiter_count--;
		unblock_proc(proc);
	}
	spinlock_release(mutex->lock);
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