#include "fork.h"
#include "arch.h"
#include "scheduler.h"
#include "memseg.h"
#include "paging.h"
#include "print.h"
#include "string.h"

pid_t fork(void){
	kdebugf("because of scheduler rewrite fork() don't work\n");
	//panic("fork() uniplemented",NULL);
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

	child->cwd_node = vfs_dup(parent->cwd_node);
	child->cwd_path = strdup(parent->cwd_path);

	//setup the return frame for the child
	fault_frame frame = *(get_current_proc()->syscall_frame);
	
	//return 0 to the child
	frame.rax = 0;

	//push the context to the child
	proc_push(child,&frame,sizeof(frame));

	//make it ruuuunnnnn !!!
	unblock_proc(child);

	return child->pid;
}