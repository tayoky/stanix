#include <kernel/scheduler.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/paging.h>
#include <kernel/string.h>
#include <kernel/list.h>
#include <kernel/arch.h>
#include <kernel/time.h>
#include <kernel/asm.h>
#include <kernel/signal.h>
#include <kernel/memseg.h>
#include <stdatomic.h>
#include <errno.h>

static process *running_proc_tail;
static process *running_proc_head;
list *proc_list;
process *sleeping_proc;

process *idle;
process *init;

static void idle_task(){
	for(;;){
		//if there are other process runnnig block us
		if(get_current_proc()->next != running_proc_head){
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
	kernel_task->flags = PROC_FLAG_PRESENT | PROC_FLAG_RUN;
	kernel_task->child = new_list();
	kernel_task->memseg = new_list();
	kernel_task->umask = 022;

	//setup a new stack
	kernel_task->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);
	set_kernel_stack(KSTACK_TOP(kernel_task->kernel_stack));

	//get the address space
	kernel_task->addrspace = get_addr_space();
	
	//let just the boot kernel task start with a cwd at initrd root
	kernel_task->cwd_node = vfs_open("/",VFS_READONLY);
	kernel_task->cwd_path = strdup("");

	//the current task is the kernel task
	kernel->current_proc = kernel_task;

	list_append(proc_list,kernel_task);

	running_proc_head = running_proc_tail = NULL;

	//the first task will be the init task
	init = get_current_proc();
	kernel->created_proc_count = 1;

	//activate task switch
	kernel->can_task_switch = 1;


	kok();

	//start idle task
	idle = new_kernel_task(idle_task,0,NULL);
}

process *schedule(){
	//pop the next task from the queue
	process *picked = running_proc_tail;
	running_proc_tail = running_proc_tail->next;
	if(!running_proc_tail)running_proc_head = NULL;

	//see if we can wakeup anything
	while(sleeping_proc){
		if(sleeping_proc->wakeup_time.tv_sec > time.tv_sec || (sleeping_proc->wakeup_time.tv_sec == time.tv_sec && sleeping_proc->wakeup_time.tv_usec > time.tv_usec)){
			break;
		}
		unblock_proc(sleeping_proc);
		sleeping_proc = sleeping_proc->snext;
	}
	
	//kdebugf("switch to %p\n",get_current_proc());
	return picked;
}

process *new_proc(){
	//init the new proc
	process *proc = kmalloc(sizeof(process));
	memset(proc,0,sizeof(process));
	proc->pid =  atomic_fetch_add(&kernel->created_proc_count,1);

	kdebugf("new proc 0x%p pid : %ld\n",proc,proc->pid);
	init_mutex(&proc->sig_lock);
	proc->addrspace = create_addr_space();
	proc->parent = get_current_proc();
	proc->flags  = PROC_FLAG_PRESENT | PROC_FLAG_BLOCKED;
	proc->child  = new_list();
	proc->memseg = new_list();
	proc->uid    = get_current_proc()->uid;
	proc->euid   = get_current_proc()->euid;
	proc->suid   = get_current_proc()->suid;
	proc->gid    = get_current_proc()->gid;
	proc->egid   = get_current_proc()->egid;
	proc->sgid   = get_current_proc()->sgid;
	proc->umask  = get_current_proc()->umask;

	//setup a new kernel stack
	proc->kernel_stack     = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);
	
	
	SP_REG(proc->context.frame) = KSTACK_TOP(proc->kernel_stack);
	kdebugf("current rsp :%p\n",SP_REG(proc->context.frame));

	//add it the the list of the childreen of the parent
	list_append(proc->parent->child,proc);

	//add it to the global process list
	list_append(proc_list,proc);

	return proc;
}

//TODO arg don't work
process *new_kernel_task(void (*func)(uint64_t,char**),uint64_t argc,char *argv[]){
	process *proc = new_proc();
	(void)argc;(void)argv;

	#ifdef __x86_64__
	asm volatile("pushfq\n"
		"pop %0" : "=r" (proc->context.frame.flags));
	proc->context.frame.cs = 0x08;
	proc->context.frame.ss = 0x10;
	proc->context.frame.ds = 0x10;
	proc->context.frame.es = 0x10;
	proc->context.frame.gs = 0x10;
	proc->context.frame.fs = 0x10;
	proc->context.frame.rip = (uint64_t)func;
	#endif

	//just copy the cwd of the current task
	proc->cwd_node = vfs_dup(get_current_proc()->cwd_node);
	proc->cwd_path = strdup(get_current_proc()->cwd_path);

	//created process are blocked until with unblock them
	unblock_proc(proc);

	return proc;
}

/// @brief push an task at the top of the queue
/// @param proc 
static void push_task(process *proc){
	if(running_proc_head){
		running_proc_head->next = proc;
	} else {
		running_proc_tail = proc;
	}

	running_proc_head = proc;

	proc->next = NULL;
}

void yield(int addback){
	if(!kernel->can_task_switch)return;
	
	int prev_int = have_interrupt();
	disable_interrupt();

	if(addback)push_task(get_current_proc());

	//save old task
	process *old = get_current_proc();
	process *new = schedule();

	//the old task isen't running anymore
	atomic_fetch_and(&old->flags,~PROC_FLAG_RUN);

	//but the new one is ! 
	atomic_fetch_or(&new->flags,PROC_FLAG_RUN);
	kernel->current_proc = new;

	//set the blocked flag if needed
	//we can't set it in block_proc cause the process is still running
	//and setting the blocked flag while the proc is running could lead to an process un blocking us
	//before we are even blocked
	if(!addback){
		atomic_fetch_or(&old->flags,PROC_FLAG_BLOCKED);
	}

	if(!new->addrspace){
		kdebugf("pid : %ld proc : %p : cr3 is 0 !!!\n",new->pid,new);
	}

	if(old->addrspace != new->addrspace){
		set_addr_space(new->addrspace);
	}

	set_kernel_stack(KSTACK_TOP(new->kernel_stack));
	
	if(new != old){
		context_switch(&old->context,&get_current_proc()->context);
	}
	if(prev_int)enable_interrupt();
}

process *get_current_proc(){
	return kernel->current_proc;
}

static void alert_parent(process *proc){
	if(!proc->parent)return;
	if((atomic_load(&proc->parent->flags) & PROC_FLAG_WAIT) && (proc->parent->waitfor == proc->pid || proc->parent->waitfor == -1 || proc->parent->waitfor == -proc->group)){
		proc->parent->waker = proc;
		unblock_proc(proc->parent);
	} else {
		send_sig(proc->parent,SIGCHLD);
	}
}

void kill_proc(void){
	disable_interrupt();
	spinlock_acquire(&get_current_proc()->state_lock);

	//all the childreen become orphelan
	//the parent of orphelan is init
	foreach(node,get_current_proc()->child){
		process *child = node->value;

		//we prevent the child from diying between when we set the parent and when we signal
		//wich could lead to a race condition
		spinlock_acquire(&child->state_lock);
		child->parent = init;
		list_append(init->child,child);
		if(child->flags & PROC_FLAG_ZOMBIE)alert_parent(child);
		spinlock_acquire(&child->state_lock);
	}
	free_list(get_current_proc()->child);
	
	//close every open fd
	for (size_t i = 0; i < MAX_FD; i++){
		if(get_current_proc()->fds[i].present){
			vfs_close(get_current_proc()->fds[i].node);
		}
	}
	
	//close cwd
	vfs_close(get_current_proc()->cwd_node);
	kfree(get_current_proc()->cwd_path);
	
	//unmap everything
	foreach(node,get_current_proc()->memseg){
		memseg_unmap(get_current_proc(),node->value);
	}
	free_list(get_current_proc()->memseg);
	
	
	alert_parent(get_current_proc());
	
	atomic_fetch_or(&get_current_proc()->flags,PROC_FLAG_ZOMBIE);
	spinlock_release(&get_current_proc()->state_lock);

	//FIXME : not SMP safe
	//another task could waitpid on us and free us between spinlock_release and block_proc
	//which is a RACE CONDITION

	block_proc();
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

int block_proc(void){
	//clear the signal interrupt flags
	atomic_fetch_and(&get_current_proc()->flags,~PROC_FLAG_INTR);
	
	int save = have_interrupt();
	disable_interrupt();

	//kdebugf("block %ld\n",get_current_proc()->pid);
	//if this is the last process unblock the idle task
	if(!running_proc_tail){
		unblock_proc(idle);
	}


	kernel->can_task_switch = 1;

	//yeld will set the blocked flag for us
	yield(0);

	if(save)enable_interrupt();

	//if we were interrupted return -EINTR
	if(atomic_load(&get_current_proc()->flags) & PROC_FLAG_INTR){
		return -EINTR;
	}

	return 0;
}

void unblock_proc(process *proc){
	//aready unblocked ?
	if(!(atomic_fetch_and(&proc->flags,~PROC_FLAG_BLOCKED) & PROC_FLAG_BLOCKED)){
		return;
	}

	int save = have_interrupt();
	disable_interrupt();

	push_task(proc);

	if(save)enable_interrupt();
}


void final_proc_cleanup(process *proc){
	list_remove(proc_list,proc);
	if(proc->parent)list_remove(proc->parent->child,proc);
	//now we can free the paging tables
	delete_addr_space(proc->addrspace);
	kfree((void*)proc->kernel_stack);
	kfree(proc);
}