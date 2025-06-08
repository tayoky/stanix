#include <kernel/cleaner.h>
#include <kernel/kernel.h>
#include <kernel/paging.h>
#include <kernel/scheduler.h>
#include <kernel/print.h>
#include <kernel/memseg.h>

void cleaner_task(){
	kstatus("start cleaner task\n");

	///go trought each task
	block_proc();

	for(;;){
		while(to_clean_proc->frist_node){
			free_proc(to_clean_proc->frist_node->value);
			list_remove(to_clean_proc,to_clean_proc->frist_node->value);
		}
		block_proc();
	}
}

void free_proc(process *proc){
	//now free the paging tables
	delete_addr_space(proc->addrspace);

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

	//free child list
	free_list(proc->child);

	//free kernel stack
	kfree((void *)proc->kernel_stack);

	kfree(proc);
}