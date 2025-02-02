#include "scheduler.h"
#include "kernel.h"
#include "print.h"
#include "kheap.h"
#include "paging.h"

void test_thread(){
	kdebugf("hello from other thread !!!!\n");
	while (1);
}

void init_task(){
	kstatus("init kernel task... ");
	//init the kernel task
	process *kernel_task = kmalloc(sizeof(process));
	kernel_task->parent = kernel_task;
	kernel_task->pid = 0;
	kernel_task->next = kernel_task;
	kernel_task->state = PROC_STATE_PRESENT | PROC_STATE_RUN;

	//get the cr3
	asm volatile("mov %%cr3, %0" : "=r" (kernel_task->cr3));

	//the current task is the kernel task
	kernel->current_proc = kernel_task;

	//activate task switch
	kernel->can_task_switch = 1;

	kok();

	new_kernel_task(test_thread);
}

void schedule(){
	//only if can task switch
	if(!kernel->can_task_switch){
		return;
	}
	do{
		kernel->current_proc = kernel->current_proc->next;
	} while (!kernel->current_proc->state & PROC_STATE_RUN);
}

process *new_proc(){
	//init the new proc
	process *proc = kmalloc(sizeof(process));
	proc->pid = ++kernel->created_proc_count;
	proc->cr3 = ((uintptr_t)init_PMLT4(kernel)) - kernel->hhdm;
	proc->parent = get_current_proc();
	proc->state = PROC_STATE_PRESENT;

	//put it just after the current task
	proc->next = get_current_proc()->next;
	get_current_proc()->next = proc;

	return proc;
}

process *new_kernel_task(void (*func)){
	process *proc = new_proc();
	proc->rsp = KERNEL_STACK_TOP;

	proc_push(proc,0x10); //ss
	kdebugf("test\n");
	proc_push(proc,KERNEL_STACK_TOP); //rsp
	proc_push(proc,0x204); //flags
	proc_push(proc,0x08); //cs
	proc_push(proc,(uint64_t)func); //rip

	//push 0 for all 15 registers
	for (size_t i = 0; i < 15; i++){
		proc_push(proc,0);
	}

	proc->state |= PROC_STATE_RUN;

	return proc;
}

void proc_push(process *proc,uint64_t value){
	//update the stack pointer
	proc->rsp -= sizeof(uint64_t);

	//find the address to write to
	uint64_t *PMLT4 = (uint64_t *)(proc->cr3 + kernel->hhdm);
	uint64_t *address = (uint64_t *) (((uintptr_t) PMLT4_virt2phys(PMLT4,(void *)proc->rsp)) + kernel->hhdm);
	kdebugf("address : 0x%lx\n",address);

	//and write to it
	*address = value;
}

process *get_current_proc(){
	return kernel->current_proc;
}