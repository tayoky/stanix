#include "fork.h"
#include "scheduler.h"
#include "memseg.h"
#include "paging.h"
#include "print.h"

pid_t fork(void){
	process *parent = get_current_proc();
	process *child = new_proc();
	child->parent = parent;

	memseg *current_seg = parent->first_memseg;
	while(current_seg){
		memseg_clone(parent,child,current_seg);
		current_seg = current_seg->next;
	}

	//clone metadata
	child->rsp = KERNEL_STACK_TOP - 8;
	child->heap_end = parent->heap_end;
	child->heap_start = parent->heap_start;
	
	kdebugf("rax : %ld\n",parent->syscall_frame[13]);

	//setup the return frame for the child
	for (size_t i = 0; i < 21; i++){
		//kdebugf("%lx\n",parent->syscall_frame[20 - i]);
		if(i == 7){
			//force rax to be 0
			proc_push(child,0);
			continue;
		}
		if(i == 4){
			kdebugf("rip : %lx",*(uint64_t *)parent->syscall_frame[20 - i]);
		}
		proc_push(child,parent->syscall_frame[20 - i]);
	}

	//flags
	//child->flags = parent->flags;
	return child->pid;
}