#include "cleaner.h"
#include "kernel.h"
#include "paging.h"
#include "scheduler.h"
#include "print.h"

void cleaner_task(){
	kstatus("start cleaner task\n");
	//go trought each task
	process *cur = get_current_proc();
	process *prev = cur;

	for(;;){
		cur = cur->next;
		//if the task is dead free it
		if(cur->flags & PROC_STATE_DEAD){
			free_proc(cur,prev);
		}
		prev = cur;
	}
}

void free_proc(process *proc,process *prev){
	//first remove it from the list
	prev->next = proc->next;

	//now free the paging tables
	delete_PMLT4((uint64_t *)(proc->cr3 + kernel->hhdm));

	//and then free the  process struct
	kfree(proc);
}