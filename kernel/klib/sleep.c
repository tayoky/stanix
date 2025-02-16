#include "sleep.h"
#include "kernel.h"
#include "scheduler.h"

void sleep_until(struct timeval wakeup_time){
	get_current_proc()->wakeup_time = wakeup_time;
	get_current_proc()->flags |= PROC_STATE_SLEEP;
	asm("int 31");
}

void sleep(long seconds){
	return micro_sleep(seconds * 1000);
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