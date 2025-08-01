#include <kernel/fork.h>
#include <kernel/arch.h>
#include <kernel/scheduler.h>
#include <kernel/memseg.h>
#include <kernel/paging.h>
#include <kernel/print.h>
#include <kernel/string.h>

pid_t fork(void){
	process *parent = get_current_proc();
	process *child = new_proc();
	child->parent = parent;

	kdebugf("forking child : %ld\n",child->pid);

	foreach(node,parent->memseg){
		memseg_clone(parent,child,node->value);
	}

	//clone metadata
	child->heap_end = parent->heap_end;
	child->heap_start = parent->heap_start;
	child->sig_mask = parent->sig_mask;
	memcpy(child->sig_handling,parent->sig_handling,sizeof(parent->sig_handling));

	//clone fd
	for(int i = 0;i<MAX_FD;i++){
		child->fds[i] = parent->fds[i];
		if(child->fds[i].present){
			child->fds[i].node   = vfs_dup(parent->fds[i].node);
			child->fds[i].offset = parent->fds[i].offset;
		}
	}

	child->cwd_node = vfs_dup(parent->cwd_node);
	child->cwd_path = strdup(parent->cwd_path);

	//setup the return frame for the child
	fault_frame frame = *(get_current_proc()->syscall_frame);
	
	//return 0 to the child
	RET_REG(frame) = 0;

	//push the context to the child
	proc_push(child,&frame,sizeof(frame));

	//make it ruuuunnnnn !!!
	unblock_proc(child);

	return child->pid;
}