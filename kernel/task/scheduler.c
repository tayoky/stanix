#include <kernel/scheduler.h>
#include <kernel/spinlock.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/kheap.h>
#include <kernel/mmu.h>
#include <kernel/string.h>
#include <kernel/list.h>
#include <kernel/arch.h>
#include <kernel/time.h>
#include <kernel/asm.h>
#include <kernel/signal.h>
#include <kernel/vmm.h>
#include <stdatomic.h>
#include <errno.h>

static run_queue_t main_run_queue;
list_t proc_list;
list_t task_list;
list_t sleeping_tasks;
spinlock_t sleep_lock;

static process_t *kernel_proc;
static process_t *init;
static task_t *idle;

static void idle_task() {
	for (;;) {
		block_prepare();
		yield(0);
	}
}

void init_task() {
	kstatusf("init kernel task... ");
	// init the scheduler first
	init_list(&proc_list);
	init_list(&task_list);
	init_list(&sleeping_tasks);

	// init the boot task (task running since boot)
	process_t *boot_task = kmalloc(sizeof(process_t));
	memset(boot_task, 0, sizeof(process_t));
	boot_task->parent = boot_task;
	boot_task->pid = 0;
	init_list(&boot_task->child);
	init_list(&boot_task->threads);
	boot_task->umask = 022;

	// get the address space
	boot_task->vmm_space.addrspace = mmu_get_addr_space();

	boot_task->main_thread = new_task(boot_task, NULL, NULL);
	boot_task->main_thread->status = TASK_STATUS_RUNNING;
	arch_set_kernel_stack(KSTACK_TOP(boot_task->main_thread->kernel_stack));

	// let just the boot kernel task start with a cwd at initrd root
	boot_task->cwd_node = vfs_get_node("/", O_RDONLY);
	boot_task->cwd_path = strdup("");

	// the current task is the boot task
	kernel->current_task = boot_task->main_thread;

	list_append(&proc_list, &boot_task->proc_list_node);

	// the first task will be the init task
	init = get_current_proc();
	kernel->tid_count = 1;

	// activate task switch
	kernel->can_task_switch = 1;

	// setup the kernel proc and the idle task
	kernel_proc = new_proc(idle_task, NULL);
	idle = kernel_proc->main_thread;

	kok();
}

static run_queue_t *get_run_queue(void) {
	return &main_run_queue;
}

static void run_queue_push_task(run_queue_t *run_queue, task_t *task) {
	list_append(&run_queue->tasks, &task->run_list_node);
}

static task_t *run_queue_pop_task(run_queue_t *run_queue) {
	if (!run_queue->tasks.first_node) return NULL;
	task_t *task = container_from_node(task_t*, run_list_node, run_queue->tasks.first_node);
	list_remove(&run_queue->tasks, &task->run_list_node);
	return task;
}

static void run_queue_acquire_lock(task_t *task) {
	for (;;) {
		run_queue_t *queue = atomic_load(&task->run_queue);
		if (!queue) return;
		spinlock_acquire(&queue->lock);
		if (atomic_load(&task->run_queue) == queue) {
			// the task didn't switch queue
			break;
		}

		// the task switched queue
		spinlock_release(&queue->lock);
	}
}

static void run_queue_release_lock(task_t *task) {
	if (task->run_queue) spinlock_release(&task->run_queue->lock);
}

static task_t *schedule() {
	// pop the next task from the queue
	task_t *picked = run_queue_pop_task(get_run_queue());

	// pick idle when nothing
	if (!picked) {
		picked = idle;
	}

	// see if we can wakeup anything
	spinlock_acquire(&sleep_lock);
	foreach (node, &sleeping_tasks) {
		task_t *thread = container_from_node(task_t*, waiter_list_node, node);
		if (thread->wakeup_time.tv_sec > time.tv_sec || (thread->wakeup_time.tv_sec == time.tv_sec && thread->wakeup_time.tv_usec > time.tv_usec)) {
			break;
		}
		atomic_fetch_and(&thread->flags, ~TASK_FLAG_SLEEP);
		list_remove(&sleeping_tasks, &thread->waiter_list_node);
		unblock_task(thread);
	}
	spinlock_release(&sleep_lock);
	
	//kdebugf("switch to %p\n",get_current_proc());
	return picked;
}

/**
 * @brief called the first time a task is executed
 */
static void new_task_trampoline(void (*func)(void *arg), void *arg) {
	finish_yield();

	// the task with interrupt disabled to avoid chaos
	// enable it ourself
	enable_interrupt();

	func(arg);
	kill_task();
}

task_t *new_task(process_t *proc, void (*func)(void *arg), void *arg) {
	task_t *task = kmalloc(sizeof(task_t));
	memset(task, 0, sizeof(task_t));

	task->tid    = atomic_fetch_add(&kernel->tid_count, 1);
	task->status = TASK_STATUS_BLOCKED;

	kdebugf("new task 0x%p tid : %ld\n", task, task->tid);

	// setup a new kernel stack
	task->kernel_stack     = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);

	task->process = proc;

	list_append(&proc->threads, &task->task_list_node);
	list_append(&task_list, &task->task_list_node);

	// setup registers
	SP_REG(task->context.frame) = KSTACK_TOP(task->kernel_stack) - 8;
	PC_REG(task->context.frame) = (uintptr_t)new_task_trampoline;
	ARG1_REG(task->context.frame) = (uintptr_t)func;
	ARG2_REG(task->context.frame) = (uintptr_t)arg;

	// TODO : move this to arch specific stuff
#ifdef __x86_64__
	task->context.frame.flags = 0x02;
	task->context.frame.cs = 0x08;
	task->context.frame.ss = 0x10;
	task->context.frame.ds = 0x10;
	task->context.frame.es = 0x10;
	task->context.frame.gs = 0x10;
	task->context.frame.fs = 0x10;
#endif

	return task;
}

process_t *new_proc(void (*func)(void *arg), void *arg){
	//init the new proc
	process_t *proc = kmalloc(sizeof(process_t));
	memset(proc, 0, sizeof(process_t));
	
	proc->parent  = get_current_proc();
	vmm_init_space(&proc->vmm_space);
	init_list(&proc->child);
	init_list(&proc->threads);
	proc->uid     = get_current_proc()->uid;
	proc->uid     = get_current_proc()->uid;
	proc->euid    = get_current_proc()->euid;
	proc->suid    = get_current_proc()->suid;
	proc->gid     = get_current_proc()->gid;
	proc->egid    = get_current_proc()->egid;
	proc->sgid    = get_current_proc()->sgid;
	proc->umask   = get_current_proc()->umask;
	proc->main_thread = new_task(proc, func, arg);
	proc->pid =  proc->main_thread->tid;

	// add it the the list of the childreen of the parent
	list_append(&proc->parent->child, &proc->child_list_node);

	// add it to the global process list
	list_append(&proc_list, &proc->proc_list_node);

	return proc;
}

task_t *new_kernel_task(void (*func)(void *arg), void *arg) {
	task_t *task = new_task(kernel_proc, func, arg); 

	// created task are blocked until with unblock them
	unblock_task(task);

	return task;
}

void finish_yield(void) {
	// the old task is not running anymore
	atomic_store(&get_run_queue()->prev->run_queue, NULL);

	spinlock_raw_release(&get_run_queue()->lock);
}

void yield(int preempt) {
	if (!kernel->can_task_switch && preempt)return;
	
	int prev_int = have_interrupt();
	disable_interrupt();

	spinlock_acquire(&get_run_queue()->lock);

	// we when preempt we continue to run an ignore status
	if (preempt || get_current_task()->status == TASK_STATUS_RUNNING) {
		run_queue_push_task(get_run_queue(), get_current_task());
	}
	
	// save old task
	task_t *old = get_current_task();
	task_t *new = schedule();
	get_run_queue()->prev = old;
	
	if (arch_save_context(&old->context)) {
		finish_yield();
		if (prev_int) enable_interrupt();
		return;
	}
	
	
	// set the new task as running
	atomic_store(&new->run_queue, get_run_queue());
	kernel->current_task = new;

	if (old->process->vmm_space.addrspace != new->process->vmm_space.addrspace) {
		mmu_set_addr_space(new->process->vmm_space.addrspace);
	}

	if (new != old) {
		arch_set_kernel_stack(KSTACK_TOP(new->kernel_stack));
		arch_load_context(&new->context);
	}
	
	finish_yield();
	if (prev_int) enable_interrupt();
	return;
}

task_t *get_current_task(void) {
	return kernel->current_task;
}

static void alert_parent(process_t *proc) {
	if (!proc->parent)return;
	if (!proc->main_thread->waiter)send_sig(proc->parent, SIGCHLD);
}

void kill_proc() {
	// just kill the main thread
	if (get_current_task() == get_current_proc()->main_thread) {
		// we are the main thread, diying will kill the proc
		kill_task();
	} else {
		// we are not the main thread
		send_sig_task(get_current_proc()->main_thread, SIGKILL);
		kill_task();
	}
}

static void do_proc_deletion(void) {
	// all the childreen become orphelan
	// the parent of orphelan is init
	foreach (node, &get_current_proc()->child) {
		process_t *child = container_from_node(process_t*, child_list_node, node);

		// we prevent the child from diying between when we set the parent and when we signal
		// wich could lead to a race condition
		spinlock_acquire(&child->main_thread->state_lock);
		child->parent = init;
		list_append(&init->child, &child->child_list_node);
		if (child->main_thread->status == TASK_STATUS_ZOMBIE) alert_parent(child);
		spinlock_release(&child->main_thread->state_lock);
	}
	destroy_list(&get_current_proc()->child);

	// close every open fd
	for (size_t i = 0; i < MAX_FD; i++) {
		if (get_current_proc()->fds[i].present) {
			vfs_close(get_current_proc()->fds[i].fd);
		}
	}

	// close cwd
	vfs_close_node(get_current_proc()->cwd_node);
	kfree(get_current_proc()->cwd_path);
	
	vmm_unmap_all();

	destroy_list(&get_current_proc()->threads);
}

void kill_task(void) {
	disable_interrupt();
	spinlock_acquire(&get_current_task()->state_lock);

	if (get_current_task() == get_current_proc()->main_thread) {
		// we are the main thread, we need to kill the whole proc
		// TODO : send SIGKILL to all threads and wait for it to be handled
		do_proc_deletion();
		alert_parent(get_current_proc());
	}

	// if a task is waiting on us alert
	if (atomic_load(&get_current_task()->waiter)) {
		// FIXME : not SMP safe
		// a task could be waiting on multiples threads and if they all wakeup the waiter at the same time
		// waker will still indicate only the last
		// RACE CONDITION
		get_current_task()->waiter->waker = get_current_task();
		unblock_task(get_current_task()->waiter);
	}

	get_current_task()->status = TASK_STATUS_ZOMBIE;
	spinlock_release(&get_current_task()->state_lock);

	// FIXME : not SMP safe
	// another task could waitpid on us and free us between spinlock_release and yield
	// which is a RACE CONDITION
	yield(0);
	__builtin_unreachable();
}

process_t *pid2proc(pid_t pid) {
	//is it ourself ?
	if (get_current_proc()->pid == pid) {
		return get_current_proc();
	}

	foreach (node, &proc_list) {
		process_t *proc = container_from_node(process_t*, proc_list_node, node);
		if (proc->pid == pid) {
			return proc;
		}
	}

	return NULL;
}

task_t *tid2task(pid_t tid) {
	// is it ourself ?
	if (get_current_task()->tid == tid) {
		return get_current_task();
	}

	foreach (node, &task_list) {
		task_t *thread = container_from_node(task_t*, task_list_node, node);
		if (thread->tid == tid) {
			return thread;
		}
	}
	return NULL;
}


int block_task(void) {
	// clear the signal interrupt flags
	atomic_fetch_and(&get_current_task()->flags, ~TASK_FLAG_INTR);

	int save = have_interrupt();
	disable_interrupt();

	// kdebugf("block %ld\n",get_current_proc()->pid);

	kernel->can_task_switch = 1;

	yield(0);

	if (save)enable_interrupt();

	// if we were interrupted return -EINTR
	if (atomic_load(&get_current_task()->flags) & TASK_FLAG_INTR) {
		return -EINTR;
	}

	return 0;
}

int unblock_task(task_t *task) {
	spinlock_acquire(&task->state_lock);
	run_queue_acquire_lock(task);
	
	// aready unblocked ?
	if (task->status != TASK_STATUS_BLOCKED) {
		run_queue_release_lock(task);
		spinlock_release(&task->state_lock);
		return 0;
	}
	task->status = TASK_STATUS_RUNNING;

	// if the task is already running on another cpu don't push it back
	// FIXME : this does not guarantee the task is not in another queue
	// just quaratee it is not being executed
	// RACE CONDITION
	// TODO : maybee make run_queue always point to the queue of task no matter what ???
	if (task->run_queue) {
		run_queue_release_lock(task);
		spinlock_release(&task->state_lock);
		return 0;
	}

	run_queue_push_task(get_run_queue(), task);

	run_queue_release_lock(task);
	spinlock_release(&task->state_lock);
	return 1;
}

void final_task_cleanup(task_t *thread) {
	list_remove(&task_list, &thread->task_list_node);
	kfree((void *)thread->kernel_stack);
	kfree(thread);
}

void final_proc_cleanup(process_t *proc) {
	list_remove(&proc_list, &proc->proc_list_node);
	if (proc->parent)list_remove(&proc->parent->child, &proc->child_list_node);

	final_task_cleanup(proc->main_thread);

	// now we can free the paging tables
	vmm_destroy_space(&proc->vmm_space);
	kfree(proc);
}

int add_fd(vfs_fd_t *fd) {
	int index = 0;
	while (is_valid_fd(index)) {
		index++;
	}

	if (index >= MAX_FD) {
		//to much fd open
		return -1;
	}

	FD_GET(index).present = 1;
	FD_GET(index).fd = fd;
	FD_GET(index).flags = 0;
	FD_GET(index).offset = 0;
	return index;
}
