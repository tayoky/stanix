#include "scheduler.h"
#include "kernel.h"
#include "print.h"
#include "kheap.h"
#include "paging.h"
#include "cleaner.h"
#include "string.h"
#include "list.h"


process *running_proc;
list *proc_list;
list *sleeping_proc;
list *to_clean_proc;

process *idle;
process *cleaner;

static void idle_task(){
	for(;;){
		//if there are other process runnnig block us
		if(get_current_proc()->next != get_current_proc()){
			block_proc();
		}
		yeld();
	}
}

void init_task(){
	kstatus("init kernel task... ");
	//init the scheduler first
	sleeping_proc = new_list();
	proc_list     = new_list();
	to_clean_proc = new_list();
	
	//init the kernel task
	process *kernel_task = kmalloc(sizeof(process));
	kernel_task->parent = kernel_task;
	kernel_task->pid = 0;
	kernel_task->next = kernel_task;
	kernel_task->prev = kernel_task;
	kernel_task->flags = PROC_STATE_PRESENT | PROC_STATE_RUN;
	kernel_task->child = new_list();

	//get the cr3
	asm volatile("mov %%cr3, %0" : "=r" (kernel_task->cr3));
	
	//let just the boot kernel task start with a cwd at initrd root
	//low effort but it work okay ?
	kernel_task->cwd_node = vfs_open("initrd:/",VFS_READONLY);
	kernel_task->cwd_path = strdup("initrd:/");

	//the current task is the kernel task
	kernel->current_proc = kernel_task;

	list_append(proc_list,kernel_task);

	running_proc = kernel_task;

	//activate task switch
	kernel->can_task_switch = 1;

	kok();

	//start the cleaner task
	cleaner = new_kernel_task(cleaner_task,0,NULL);

	//start idle task
	idle = new_kernel_task(idle_task,0,NULL);
}

void schedule(){
	//only if can task switch
	if(!kernel->can_task_switch){
		return;
	}

	//get the next task
	kernel->current_proc = running_proc->next;
	//kdebugf("switch to %p\n",get_current_proc());
}

process *new_proc(){
	//init the new proc
	process *proc = kmalloc(sizeof(process));
	memset(proc,0,sizeof(process));
	proc->pid = ++kernel->created_proc_count;
	proc->cr3 = ((uintptr_t)init_PMLT4(kernel)) - kernel->hhdm;
	proc->parent = get_current_proc();
	proc->flags = PROC_STATE_PRESENT;
	proc->child = new_list();

	//add it to the global process list
	list_append(proc_list,proc);

	return proc;
}

static void set_running(process *proc){
	yeld();
	proc->next = get_current_proc();
	proc->prev = get_current_proc()->prev;
	get_current_proc()->prev->next = proc;
	get_current_proc()->prev = proc;
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
	proc->cwd_node = vfs_dup(get_current_proc()->cwd_node);
	proc->cwd_path = strdup(get_current_proc()->cwd_path);

	proc->flags |= PROC_STATE_RUN;

	//put it into the running process list
	//just before us
	set_running(proc);

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
	//is the parent waiting ?
	if(proc->parent && (proc->parent->flags & PROC_STATE_WAIT)){
		//see if we can wake it up
		if(proc->parent->waitfor == proc->pid || proc->parent->waitfor == -1){
			unblock_proc(proc->parent);
		}
	}
	proc->flags = PROC_STATE_PRESENT | PROC_STATE_ZOMBIE | PROC_STATE_TOCLEAN;

	list_append(to_clean_proc,proc);

	//if the proc is it self
	//then we have to yeld
	if(proc == get_current_proc()){
		yeld();
		for(;;);
	}
}

process *pid2proc(pid_t pid){
	//is it ourself ?
	if(get_current_proc()->pid == pid){
		return get_current_proc();
	}

	foreach(node,proc_list){
		process *proc = node->value;
		if(proc->pid == pid){
			return proc;
		}
	}

	return NULL;
}

void block_proc(){
	//if this is the last process unblock the idle task
	if(get_current_proc()->next == get_current_proc()){
		unblock_proc(idle);
	}
	//block ourself
	get_current_proc()->flags &= ~(uint64_t)PROC_STATE_RUN;

	//remove us from the list
	get_current_proc()->prev->next = get_current_proc()->next;
	get_current_proc()->next->prev = get_current_proc()->prev;

	//apply
	yeld();
}

void unblock_proc(process *proc){
	//aready unblock ?
	if(proc->flags & PROC_STATE_RUN){
		return;
	}
	proc->flags |= PROC_STATE_RUN;
	set_running(proc);
}