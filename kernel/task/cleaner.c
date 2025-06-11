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

	//free the used space
	memseg *current_memseg = proc->first_memseg;
	while(current_memseg){
		memseg *next = current_memseg->next;
		memseg_unmap(proc,current_memseg);
		current_memseg = next;
	}

	kfree(proc);
}