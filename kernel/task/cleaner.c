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

	
	//free all the heap
	uint64_t num_pages = PAGE_ALIGN_UP(proc->heap_end - proc->heap_start) / PAGE_SIZE;
	uint64_t virt_page = PAGE_ALIGN_DOWN(proc->heap_start);
	while(num_pages > 0){
		//find it's phys address
		uint64_t phys_page = (uint64_t)PMLT4_virt2phys((uint64_t *)(proc->cr3 + kernel->hhdm),(void *)PAGE_ALIGN_UP(proc->heap_end) - num_pages * PAGE_SIZE) / PAGE_SIZE;

		//and then free it
		free_page(&kernel->bitmap,phys_page);

		virt_page++;
		num_pages--;
	}

	//now free the paging tables
	delete_PMLT4((uint64_t *)(proc->cr3 + kernel->hhdm));

	//close every open fd
	for (size_t i = 0; i < MAX_FD; i++){
		if(proc->fds[i].present){
			vfs_close(proc->fds[i].node);
		}
	}

	//close cwd
	vfs_close(proc->cwd.node);

	//and then free the  process struct
	kfree(proc);
}