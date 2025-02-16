#include "scheduler.h"
#include "kernel.h"
#include "print.h"
#include "kheap.h"
#include "paging.h"
#include "cleaner.h"
#include "string.h"


void init_task(){
	kstatus("init kernel task... ");
	//init the kernel task
	process *kernel_task = kmalloc(sizeof(process));
	kernel_task->parent = kernel_task;
	kernel_task->pid = 0;
	kernel_task->next = kernel_task;
	kernel_task->flags = PROC_STATE_PRESENT | PROC_STATE_RUN;

	//get the cr3
	asm volatile("mov %%cr3, %0" : "=r" (kernel_task->cr3));
	
	//let just the boot kernel task start with a cwd at initrd root
	//low effort but it work okay ?
	kernel_task->cwd.present = 1;
	kernel_task->cwd.node = vfs_open("initrd:/",VFS_READONLY);


	//the current task is the kernel task
	kernel->current_proc = kernel_task;

	//activate task switch
	kernel->can_task_switch = 1;

	kok();

	//start the cleaner task
	new_kernel_task(cleaner_task,0,NULL);
}

void schedule(){
	//only if can task switch
	if(!kernel->can_task_switch){
		return;
	}
	do{
		kernel->current_proc = kernel->current_proc->next;

		if((kernel->current_proc->flags & PROC_STATE_SLEEP) && (kernel->current_proc->flags & PROC_STATE_RUN)){
			//the process is spleeping
			//see if we wakeup
			struct timeval wakeup_time = kernel->current_proc->wakeup_time;
			if(wakeup_time.tv_sec > time.tv_sec){
				kernel->current_proc->flags &= ~(uint64_t)PROC_STATE_SLEEP;
				break;
			}
			if((wakeup_time.tv_sec == time.tv_sec) && (wakeup_time.tv_usec > time.tv_usec)){
				kernel->current_proc->flags &= ~(uint64_t)PROC_STATE_SLEEP;
				break;
			}
			continue;
		}
	} while (!(kernel->current_proc->flags & PROC_STATE_RUN));
}

process *new_proc(){
	//init the new proc
	process *proc = kmalloc(sizeof(process));
	memset(proc,0,sizeof(process));
	proc->pid = ++kernel->created_proc_count;
	proc->cr3 = ((uintptr_t)init_PMLT4(kernel)) - kernel->hhdm;
	proc->parent = get_current_proc();
	proc->flags = PROC_STATE_PRESENT;

	//put it just after the current task
	proc->next = get_current_proc()->next;
	get_current_proc()->next = proc;

	return proc;
}

//TODO argv don't work
process *new_kernel_task(void (*func)(uint64_t,char**),uint64_t argc,char *argv[]){
	process *proc = new_proc();
	proc->rsp = KERNEL_STACK_TOP;

	proc_push(proc,0x10); //ss
	proc_push(proc,KERNEL_STACK_TOP); //rsp
	proc_push(proc,0x204); //flags
	proc_push(proc,0x08); //cs
	proc_push(proc,(uint64_t)func); //rip

	//use the two first register (rdi and rsi) for argc and argv
	proc_push(proc,argc);
	proc_push(proc,(uint64_t)argv);
 
	//push 0 for all other 13 registers
	for (size_t i = 0; i < 13; i++){
		proc_push(proc,0);
	}

	//push ds
	proc_push(proc,0x10);

	//just copy the cwd of the current task
	proc->cwd.node = vfs_dup(get_current_proc()->cwd.node);

	proc->flags |= PROC_STATE_RUN;

	return proc;
}

void proc_push(process *proc,uint64_t value){
	//update the stack pointer
	proc->rsp -= sizeof(uint64_t);

	//find the address to write to
	uint64_t *PMLT4 = (uint64_t *)(proc->cr3 + kernel->hhdm);
	uint64_t *address = (uint64_t *) (((uintptr_t) PMLT4_virt2phys(PMLT4,(void *)proc->rsp)) + kernel->hhdm);

	//and write to it
	*address = value;
}

process *get_current_proc(){
	return kernel->current_proc;
}

void kill_proc(process *proc){
	proc->flags = PROC_STATE_PRESENT | PROC_STATE_DEAD;

	//if the proc is it self
	//then we have to while until we are stoped
	if(proc == get_current_proc()){
		for(;;);
	}
}