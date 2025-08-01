#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/paging.h>
#include <kernel/cleaner.h>
#include <kernel/string.h>
#include <kernel/list.h>
#include <kernel/arch.h>
#include <kernel/time.h>
#include <kernel/asm.h>
#include <kernel/memseg.h>
#include <errno.h>

process *running_proc;
list *proc_list;
process *sleeping_proc;

process *idle;
process *init;

static void idle_task(){
	for(;;){
		//if there are other process runnnig block us
		if(get_current_proc()->next != get_current_proc()){
			block_proc();
		}
	}
}

void init_task(){
	kstatus("init kernel task... ");
	//init the scheduler first
	proc_list     = new_list();
	sleeping_proc = NULL;
	
	//init the kernel task
	process *kernel_task = kmalloc(sizeof(process));
	memset(kernel_task,0,sizeof(process));
	kernel_task->parent = kernel_task;
	kernel_task->pid = 0;
	kernel_task->next = kernel_task;
	kernel_task->prev = kernel_task;
	kernel_task->flags = PROC_FLAG_PRESENT | PROC_FLAG_RUN;
	kernel_task->child = new_list();
	kernel_task->memseg = new_list();
	kernel_task->umask = 0x2;

	//setup a new stack
	kernel_task->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
	kernel_task->kernel_stack &= ~0xFUL;

	//get the address space
	kernel_task->addrspace = get_addr_space();
	
	//let just the boot kernel task start with a cwd at initrd root
	kernel_task->cwd_node = vfs_open("/",VFS_READONLY);
	kernel_task->cwd_path = strdup("");

	//the current task is the kernel task
	kernel->current_proc = kernel_task;

	list_append(proc_list,kernel_task);

	running_proc = kernel_task;

	//the first task will be the init task
	init = get_current_proc();

	//activate task switch
	kernel->can_task_switch = 1;

	kok();

	//start idle task
	idle = new_kernel_task(idle_task,0,NULL);
}

void schedule(){
	//get the next task
	kernel->current_proc = get_current_proc()->next;

	//see if we can wakeup anything
	while(sleeping_proc){
		if(sleeping_proc->wakeup_time.tv_sec > time.tv_sec || (sleeping_proc->wakeup_time.tv_sec == time.tv_sec && sleeping_proc->wakeup_time.tv_usec > time.tv_usec)){
			break;
		}
		unblock_proc(sleeping_proc);
		sleeping_proc = sleeping_proc->snext;
	}
	
	//kdebugf("switch to %p\n",get_current_proc());
}

process *new_proc(){
	//init the new proc
	kdebugf("next : %p\n",get_current_proc()->next);
	process *proc = kmalloc(sizeof(process));
	memset(proc,0,sizeof(process));
	proc->pid = ++kernel->created_proc_count;
	kdebugf("new proc 0x%p next : 0x%p pid : %ld/%ld\n",proc,get_current_proc()->next,proc->pid,kernel->created_proc_count);
	proc->addrspace = create_addr_space();
	proc->parent = get_current_proc();
	proc->flags = PROC_FLAG_PRESENT;
	proc->child = new_list();
	proc->memseg = new_list();
	proc->uid   = get_current_proc()->uid;
	proc->euid  = get_current_proc()->euid;
	proc->suid  = get_current_proc()->suid;
	proc->gid   = get_current_proc()->gid;
	proc->egid  = get_current_proc()->egid;
	proc->sgid  = get_current_proc()->sgid;
	proc->umask = get_current_proc()->umask;

	//setup a new kernel stack
	proc->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
	proc->kernel_stack &= ~0xFUL;
	proc->rsp = proc->kernel_stack;
	kdebugf("current rsp :%p\n",proc->rsp);

	//add it the the list of the childreen of the parent
	list_append(proc->parent->child,proc);

	//add it to the global process list
	list_append(proc_list,proc);

	if(!proc->addrspace){
		kdebugf("cr3 is 0 !!!\n");
		disable_interrupt();
		for(;;);
	}

	return proc;
}

//TODO argv don't work
process *new_kernel_task(void (*func)(uint64_t,char**),uint64_t argc,char *argv[]){
	process *proc = new_proc();

	fault_frame context;
	memset(&context,0,sizeof(fault_frame));
	ARG1_REG(context) = argc;
	ARG2_REG(context) = (uintptr_t)argv;
	#ifdef x86_64
	asm volatile("pushfq\n"
		"pop %0" : "=r" (context.flags));
	context.cs = 0x08;
	context.ss = 0x10;
	context.rip = (uint64_t)func;
	context.rsp = proc->kernel_stack;
	#endif
	proc_push(proc,&context,sizeof(fault_frame));
	kdebugf("%p %p\n",proc->addrspace,proc->rsp);

	//just copy the cwd of the current task
	proc->cwd_node = vfs_dup(get_current_proc()->cwd_node);
	proc->cwd_path = strdup(get_current_proc()->cwd_path);

	//created process are blocked until with unblock them
	unblock_proc(proc);

	return proc;
}

void proc_push(process *proc,void *value,size_t size){
	//update the stack pointer
	proc->rsp -= size;

	char *buffer = value;

	for(size_t i=0; i<size; i++){
		//find the address to write to
		char *address = (char *) (((uintptr_t) space_virt2phys(proc->addrspace,(void *)(proc->rsp + i))) + kernel->hhdm);

		//and write to it
		*address = buffer[i];
	}
}

void yeld(){
	if(!kernel->can_task_switch){
		return;
	}

	kernel->can_task_switch = 0;

	//save old proc
	process *old = get_current_proc();

	schedule();

	if(!get_current_proc()->addrspace){
		kdebugf("%ld/%ld : %p : cr3 is 0 !!!\n",get_current_proc()->pid,kernel->created_proc_count,old->next);
	}

	if(old->addrspace != get_current_proc()->addrspace){
		set_addr_space(get_current_proc()->addrspace);
	}

	set_kernel_stack(get_current_proc()->kernel_stack);

	//kdebugf("rsp : %p\n",get_current_proc()->rsp);

	kernel->can_task_switch = 1;
	if(get_current_proc() != old){
		context_switch(get_current_proc()->rsp,&old->rsp);
	}
}

process *get_current_proc(){
	return kernel->current_proc;
}

void kill_proc(process *proc){
	//all the childreen become orphelan
	//the parent of orphelan is init
	foreach(node,proc->child){
		process *child = node->value;
		child->parent = init;
		list_append(init->child,child);
	}
	free_list(proc->child);

	//close every open fd
	for (size_t i = 0; i < MAX_FD; i++){
		if(proc->fds[i].present){
			vfs_close(proc->fds[i].node);
		}
	}

	//close cwd
	vfs_close(proc->cwd_node);
	kfree(proc->cwd_path);

	//unmap everything
	foreach(node,proc->memseg){
		memseg_unmap(proc,node->value);
	}

	//the tricky part begin
	kernel->can_task_switch = 0;

	//is the parent waiting ?
	if(proc->parent && (proc->parent->flags & PROC_FLAG_WAIT)){
		//see if we can wake it up
		if(proc->parent->waitfor == proc->pid || proc->parent->waitfor == -1 || proc->parent->waitfor == -proc->group){
			unblock_proc(proc->parent);
		}
	}
	
	proc->flags |= PROC_FLAG_ZOMBIE;
	block_proc(proc);
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

int block_proc(){
	//clear the intterupt flags
	get_current_proc()->flags &= ~PROC_FLAG_INTR;

	//kdebugf("block %ld\n",get_current_proc()->pid);
	//if this is the last process unblock the idle task
	if(get_current_proc()->next == get_current_proc()){
		unblock_proc(idle);
	}

	kernel->can_task_switch = 0;

	//block ourself
	get_current_proc()->flags &= ~(uint64_t)PROC_FLAG_RUN;
	get_current_proc()->flags |= PROC_FLAG_BLOCKED;

	//remove us from the list
	get_current_proc()->prev->next = get_current_proc()->next;
	get_current_proc()->next->prev = get_current_proc()->prev;

	kernel->can_task_switch = 1;
	//apply
	yeld();

	//if we were interrupted return -EINTR
	if(get_current_proc()->flags & PROC_FLAG_INTR){
		return -EINTR;
	}

	return 0;
}

void unblock_proc(process *proc){
	char old = kernel->can_task_switch;
	//aready unblocked ?
	if(proc->flags & PROC_FLAG_RUN){
		return;
	}
	//kdebugf("unblock %ld\n",proc->pid);
	proc->flags |= PROC_FLAG_RUN;
	proc->flags &= ~PROC_FLAG_BLOCKED;

	kernel->can_task_switch = 0;

	//add it just before us
	proc->next = get_current_proc();
	proc->prev = get_current_proc()->prev;
	proc->prev->next = proc;
	proc->waker = get_current_proc();
	get_current_proc()->prev = proc;

	kernel->can_task_switch = old;
}

int is_userspace(process *proc){
	return !(proc->flags & PROC_FLAG_KERNEL);
}
