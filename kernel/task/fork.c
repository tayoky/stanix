#include "fork.h"
#include "scheduler.h"
#include "memseg.h"
#include "paging.h"
#include "print.h"
#include "string.h"
#include "arch.h"

pid_t fork(void){
	kdebugf("because of scheduler rewrite fork() don't work\n");
	panic("fork() uniplemented",NULL);
	process *parent = get_current_proc();
	process *child = new_proc();
	child->parent = parent;

	kdebugf("forking child : %ld\n",child->pid);

	memseg *current_seg = parent->first_memseg;
	while(current_seg){
		memseg_clone(parent,child,current_seg);
		current_seg = current_seg->next;
	}

	//clone metadata
	child->rsp = KERNEL_STACK_TOP;
	child->heap_end = parent->heap_end;
	child->heap_start = parent->heap_start;

	//clone fd
	for(int i = 0;i<MAX_FD;i++){
		child->fds[i] = parent->fds[i];
		if(child->fds[i].present) child->fds[i].node  = vfs_dup(parent->fds[i].node);
	}

	//setup the return frame for the child
	fault_frame frame;
	
	proc_push(child,&frame,sizeof(frame));

	//flags
	child->flags = parent->flags;
	return child->pid;
}