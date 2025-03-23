#include "sleep.h"
#include "kernel.h"
#include "scheduler.h"
#include "print.h"

void sleep_until(struct timeval wakeup_time){
	kernel->can_task_switch = 0;
	kdebugf("wait until : %ld:%ld\n",wakeup_time.tv_sec,wakeup_time.tv_usec);
	get_current_proc()->wakeup_time = wakeup_time;
	get_current_proc()->flags |= PROC_STATE_SLEEP;

	//add us to the list
	//keep the list orgnazie from first awake to last
	process *proc = sleeping_proc;
	process *prev = NULL;
	while(proc){

		if(proc->wakeup_time.tv_sec > wakeup_time.tv_usec || (proc->wakeup_time.tv_sec == wakeup_time.tv_sec && proc->wakeup_time.tv_usec > wakeup_time.tv_usec)){
			break;
		}

		prev = proc;
		proc = proc->snext;
	}
	
	if(prev){
		get_current_proc()->snext = prev->snext;
		prev->snext = get_current_proc();
	} else {
		get_current_proc()->snext = sleeping_proc;
		sleeping_proc = get_current_proc();
	}
	kdebugf("sleep : %p\n",sleeping_proc);
	block_proc();
}

void sleep(long seconds){
	return micro_sleep(seconds * 1000000);
}

void micro_sleep(suseconds_t micro_second){
	if(micro_second < 100){
		return;
	}
	//caclulate new time val
	struct timeval new_timeval = time;
	if(1000000 - new_timeval.tv_usec > micro_second){
		new_timeval.tv_usec += micro_second;
	} else {
		micro_second -= 1000000 - new_timeval.tv_usec;
		new_timeval.tv_sec ++;
		new_timeval.tv_usec = 0;

		//now we need to cut the remaing micro second in second and micro second
		new_timeval.tv_sec += micro_second / 1000000;
		new_timeval.tv_usec = micro_second % 1000000;
	}

	sleep_until(new_timeval);
}