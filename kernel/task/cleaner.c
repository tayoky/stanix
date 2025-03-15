#include "cleaner.h"
#include "kernel.h"
#include "paging.h"
#include "scheduler.h"
#include "print.h"
#include "memseg.h"

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

	//close every open fd
	for (size_t i = 0; i < MAX_FD; i++){
		if(proc->fds[i].present){
			vfs_close(proc->fds[i].node);
		}
	}

	//close cwd
	vfs_close(proc->cwd_node);
	kfree(proc->cwd_path);

	//free the used space
	memseg *current_memseg = proc->first_memseg;
	while(current_memseg){
		memseg *next = current_memseg->next;
		memseg_unmap(proc,current_memseg);
		current_memseg = next;
	}

	//and then free the  process struct
	kfree(proc);
}