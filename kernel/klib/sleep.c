#include <kernel/time.h>
#include <kernel/kernel.h>
#include <kernel/scheduler.h>
#include <kernel/print.h>

int sleep_until(struct timeval wakeup_time){
	kdebugf("wait until : %ld:%ld\n",wakeup_time.tv_sec,wakeup_time.tv_usec);
	get_current_proc()->wakeup_time = wakeup_time;

	//add us to the list
	//keep the list organise from first awake to last
	//TODO : put a spinlock or something on the sleep queue
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
	return block_proc();
}

int sleep(long seconds){
	return micro_sleep(seconds * 1000000);
}

int micro_sleep(suseconds_t micro_second){
	if(micro_second < 100){
		return 0;
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

	return sleep_until(new_timeval);
}