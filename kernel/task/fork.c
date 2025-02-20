#include "fork.h"
#include "scheduler.h"
#include "memseg.h"
#include "paging.h"

pid_t fork(void){
	process *parent = get_current_proc();
	process *child = new_proc();

	memseg *current_seg = parent->first_memseg;
	while(current_seg){
		memseg_clone(parent,child,current_seg);
		current_seg = current_seg->next;
	}

	//clone metadata
	child->rsp = parent->rsp;
	child->heap_end = parent->heap_end;
	child->heap_start = parent->heap_start;

	//flags
	//child->flags = parent->flags;
	return child->pid;
}