#include "fork.h"
#include "scheduler.h"
#include "memseg.h"
#include "paging.h"
#include "print.h"

pid_t fork(void){
	kdebugf("forking\n");
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

	//clone fd
	for(int i = 0;i<MAX_FD;i++){
		child->fds[i] = parent->fds[i];
		child->fds[i].node  = vfs_dup(parent->fds[i].node);
	}
	child->cwd = parent->cwd;
	child->cwd.node = vfs_dup(parent->cwd.node);

	//setup the return frame for the child
	for (size_t i = 0; i < 21; i++){
		if(i == 7){
			//force rax to be 0
			proc_push(child,0);
			continue;
		}
		proc_push(child,parent->syscall_frame[20 - i]);
	}

	//flags
	child->flags = parent->flags;
	return child->pid;
}