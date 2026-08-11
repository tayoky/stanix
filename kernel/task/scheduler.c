#include <kernel/arch.h>
#include <kernel/asm.h>
#include <kernel/kernel.h>
#include <kernel/kheap.h>
#include <kernel/list.h>
#include <kernel/mmu.h>
#include <kernel/print.h>
#include <kernel/process.h>
#include <kernel/rwlock.h>
#include <kernel/scheduler.h>
#include <kernel/signal.h>
#include <kernel/sleep.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/time.h>
#include <kernel/vmm.h>
#include <kernel/xarray.h>
#include <errno.h>
#include <stdatomic.h>

static run_queue_t main_run_queue;
static xarray_t tasks_list;
static atomic_size_t tid_count = 1;
static char can_task_switch    = 0;
list_t sleeping_tasks;
spinlock_t sleep_lock;
spinlock_t proctree_lock;

static process_t *kernel_proc;
process_t *init;
static task_t *idle;
static task_t *reaper;
static list_t dead_tasks;
static spinlock_t dead_tasks_lock;

static run_queue_t *get_run_queue(void) {
	return &main_run_queue;
}

static int run_queue_is_empty(run_queue_t *run_queue) {
	return list_is_empty(&run_queue->tasks);
}

static void run_queue_push_task(run_queue_t *run_queue, task_t *task) {
	list_append(&run_queue->tasks, &task->run_list_node);
	atomic_store(&task->run_queue, run_queue);
}

static task_t *run_queue_pop_task(run_queue_t *run_queue) {
	if (run_queue_is_empty(run_queue)) return NULL;
	task_t *task = container_of(run_queue->tasks.first_node, task_t, run_list_node);
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

static void idle_task() {
	for (;;) {
		arch_pause();
		if (!run_queue_is_empty(get_run_queue())) {
			block_prepare();
			yield(0);
		}
	}
}

static void task_final_cleanup(task_t *task) {
	kfree((void *)task->kernel_stack);

	// the task hold a ref to the proc
	proc_release(task->process);

	// the scheduler hold a ref that we need to release
	task_release(task);
}

static void reaper_task() {
	for (;;) {
		spinlock_acquire(&dead_tasks_lock);
		list_node_t *node = dead_tasks.first_node;
		if (node) {
			list_remove(&dead_tasks, node);
		}
		spinlock_release(&dead_tasks_lock);
		if (!node) {
			block_prepare();
			block_task();
			continue;
		}
		task_t *task = container_of(node, task_t, dead_list_node);
		// make sure the task is not on a run queue
		// FIXME : is this bad ?
		while (atomic_load(&task->run_queue));
		task_final_cleanup(task);
	}
}

static void add_dead_task(task_t *task) {
	spinlock_acquire(&dead_tasks_lock);
	list_append(&dead_tasks, &task->dead_list_node);
	spinlock_release(&dead_tasks_lock);
	unblock_task(reaper);
}

void init_task() {
	kstatusf("init kernel task... ");
	// init the scheduler first
	xarray_init(&tasks_list);
	list_init(&sleeping_tasks);

	// init the boot task (task running since boot)
	process_t *boot_task = kmalloc(sizeof(process_t));
	memset(boot_task, 0, sizeof(process_t));
	boot_task->parent = boot_task;
	boot_task->pid    = 0;
	spinlock_acquire(&boot_task->proc_lock);
	boot_task->group  = process_group_get_or_create_from_pgid(0);
	list_init(&boot_task->child);
	list_init(&boot_task->threads);
	boot_task->umask = 022;
	proc_set_cred(boot_task, cred_dup(&default_cred));
	spinlock_release(&boot_task->proc_lock);

	// get the address space
	boot_task->vmm_space.addrspace = mmu_get_addr_space();

	boot_task->main_thread         = task_new(boot_task, NULL, NULL);
	boot_task->main_thread->status = TASK_STATUS_RUNNING;
	arch_set_kernel_stack(KSTACK_TOP(boot_task->main_thread->kernel_stack));

	// let just the boot kernel task start with a cwd at initrd root
	boot_task->cwd = vfs_get_dentry("/", 0);

	// the current task is the boot task
	get_run_queue()->current = boot_task->main_thread;
	set_cmdline("init");

	proc_ref(boot_task);
	proc_register(boot_task);

	// the first task will be the init task
	init = get_current_proc();

	// activate task switch
	can_task_switch = 1;

	// setup the kernel proc, the idle task and the reaper
	kernel_proc = proc_new(idle_task, NULL);
	proc_set_cmdline(kernel_proc, "stanix kernel");
	idle = kernel_proc->main_thread;
	reaper = new_kernel_task(reaper_task, NULL);

	kok();
}

static void wakeup_sleepers(void) {
	// see if we can wakeup anything
	struct timespec time;
	gettime(CLOCK_MONOTONIC, &time);
	spinlock_acquire(&sleep_lock);
	foreach (node, &sleeping_tasks) {
		task_t *task = container_of(node, task_t, waiter_list_node);
		if (timespec_cmp(&task->wakeup_time, &time) > 0) {
			break;
		}
		atomic_fetch_and(&task->flags, ~TASK_FLAG_SLEEP);
		list_remove(&sleeping_tasks, &task->waiter_list_node);
		unblock_task(task);
	}
	spinlock_release(&sleep_lock);
}

static task_t *schedule() {
	// pop the next task from the queue
	task_t *picked = run_queue_pop_task(get_run_queue());

	// pick idle when nothing
	if (!picked) {
		picked = idle;
	}

	// kdebugf("switch to %p\n",get_current_proc());
	return picked;
}

/**
 * @brief called the first time a task is executed
 */
static void task_new_trampoline(void (*func)(void *arg), void *arg) {
	finish_yield();

	// the task with interrupt disabled to avoid chaos
	// enable it ourself
	enable_interrupt();

	func(arg);
	task_exit();
}

task_t *task_new(process_t *proc, void (*func)(void *arg), void *arg) {
	task_t *task = kmalloc(sizeof(task_t));
	memset(task, 0, sizeof(task_t));

	task->tid    = atomic_fetch_add(&tid_count, 1);
	task->status = TASK_STATUS_BLOCKED;

	kdebugf("new task 0x%p tid : %ld\n", task, task->tid);

	// setup a new kernel stack
	task->kernel_stack = (uintptr_t)kmalloc(KERNEL_STACK_SIZE);

	// the task hold a ref to the proc
	task->process = proc_ref(proc);

	// the scheduler hold a ref
	// but the tasks list and proc only a weak ref
	task_ref(task);
	spinlock_acquire(&proc->proc_lock);
	proc->threads_count++;
	list_append(&proc->threads, &task->thread_list_node);
	spinlock_release(&proc->proc_lock);
	xarray_set(&tasks_list, task->tid, task);

	// inherit sigmask
	if (get_current_task()) {
		task->sig_mask   = get_current_task()->sig_mask;
	}

	// setup registers
	SP_REG(task->context.frame)   = KSTACK_TOP(task->kernel_stack) - 8;
	PC_REG(task->context.frame)   = (uintptr_t)task_new_trampoline;
	ARG1_REG(task->context.frame) = (uintptr_t)func;
	ARG2_REG(task->context.frame) = (uintptr_t)arg;

	// TODO : move this to arch specific stuff
#ifdef __x86_64__
	task->context.frame.flags = 0x02;
	task->context.frame.cs    = 0x08;
	task->context.frame.ss    = 0x10;
	task->context.frame.ds    = 0x10;
	task->context.frame.es    = 0x10;
	task->context.frame.gs    = 0x10;
	task->context.frame.fs    = 0x10;
	task->context.fpu.fcw     = 0x037f;
	task->context.fpu.mxcsr   = 0x1F80;
#endif

	return task;
}

task_t *new_kernel_task(void (*func)(void *arg), void *arg) {
	task_t *task = task_new(kernel_proc, func, arg);

	// created task are blocked until with unblock them
	unblock_task(task);

	return task;
}

void finish_yield(void) {
	if (!get_run_queue()->prev_is_on_queue) {
		// the old task is not on the run queue anymore
		atomic_store(&get_run_queue()->prev->run_queue, NULL);
	}

	spinlock_raw_release(&get_run_queue()->lock);
}

void yield(int preempt) {
	if (!can_task_switch && preempt) return;
	if (get_current_task()->preempt_disable && preempt) return;

	wakeup_sleepers();

	int prev_int = have_interrupt();
	disable_interrupt();

	spinlock_acquire(&get_run_queue()->lock);

	// we when preempt we continue to run and ignore status
	if (preempt || get_current_task()->status == TASK_STATUS_RUNNING) {
		run_queue_push_task(get_run_queue(), get_current_task());
		get_run_queue()->prev_is_on_queue = 1;
	} else {
		get_run_queue()->prev_is_on_queue = 0;
	}

	// save old task
	task_t *old           = get_current_task();
	task_t *new           = schedule();
	get_run_queue()->prev = old;

	// fast path
	if (old == new) {
		finish_yield();
		if (prev_int) enable_interrupt();
		return;
	}

	if (preempt) {
		atomic_fetch_add(&old->preempt_context_switches, 1);
	} else {
		atomic_fetch_add(&old->voluntary_context_switches, 1);
	}

	if (arch_save_context(&old->context)) {
		finish_yield();
		if (prev_int) enable_interrupt();
		return;
	}


	// set the new task as the current
	get_run_queue()->current = new;

	if (old->process->vmm_space.addrspace != new->process->vmm_space.addrspace) {
		mmu_set_addr_space(new->process->vmm_space.addrspace);
	}

	arch_set_kernel_stack(KSTACK_TOP(new->kernel_stack));
	arch_load_context(&new->context);
}

task_t *get_current_task(void) {
	return get_run_queue()->current;
}

void task_exit(void) {
	preempt_disable();

	spinlock_acquire(&get_current_proc()->proc_lock);
	get_current_proc()->threads_count--;
	int is_last = (get_current_proc()->threads_count == 0);
	spinlock_release(&get_current_proc()->proc_lock);

	if (is_last) {
		// we are the last thread, we need to kill the whole proc
		do_proc_deletion();
	}
	
	xarray_set(&tasks_list, get_current_task()->tid, NULL);
	set_task_status(TASK_STATUS_DEAD);
	add_dead_task(get_current_task());

	yield(0);
	__builtin_unreachable();
}

task_t *task_from_tid(pid_t tid) {
	// is it ourself ?
	if (get_current_task()->tid == tid) {
		return task_ref(get_current_task());
	}

	rcu_acquire_read(&tasks_list.rcu);
	task_t *task = xarray_get(&tasks_list, tid);
	task_ref(task);
	rcu_release_read(&tasks_list.rcu);
	return task;
}

int block_task(void) {
	yield(0);

	// if we were interrupted return -EINTR
	if (get_current_task()->wakeup_reason == WAKEUP_SIGNAL) {
		return -EINTR;
	}

	return 0;
}

int block_task_timeout(struct timespec *timeout) {
	if (!timeout) {
		return block_task();
	}

	// fast path
	struct timespec time;
	gettime(CLOCK_MONOTONIC, &time);
	if (timespec_cmp(timeout, &time) <= 0) {
		block_cancel();
		return -ETIMEDOUT;
	}

	sleep_add_timeout(timeout);

	// check if timeout is already out
	gettime(CLOCK_MONOTONIC, &time);
	if (timespec_cmp(timeout, &time) <= 0) {
		block_cancel();
		return -ETIMEDOUT;
	}

	yield(0);

	switch (get_current_task()->wakeup_reason) {
	case WAKEUP_TIMEOUT:
		// no need to remove from timeout list
		// because we know we waked up because of timeout
		return -ETIMEDOUT;
	case WAKEUP_SIGNAL:
		sleep_remove_timeout();
		return -EINTR;
	case WAKEUP_OTHER:
	default:
		sleep_remove_timeout();
		return 0;
	}
}

int unblock_task_reason(task_t *task, int reason) {
	spinlock_acquire(&task->state_lock);
	run_queue_acquire_lock(task);

	// aready unblocked ?
	if (task->status != TASK_STATUS_BLOCKED && task->status != TASK_STATUS_INTERRUPTIBLE) {
		run_queue_release_lock(task);
		spinlock_release(&task->state_lock);
		return 0;
	}

	// if unblocking because of a signal can only do it if interruptible
	if (reason == WAKEUP_SIGNAL && task->status != TASK_STATUS_INTERRUPTIBLE) {
		return 0;
	}

	task->status        = TASK_STATUS_RUNNING;
	task->wakeup_reason = reason;

	// if the task is already in the queue on another cpu don't push it back
	if (task->run_queue) {
		run_queue_release_lock(task);
		spinlock_release(&task->state_lock);
		return 1;
	}

	run_queue_push_task(get_run_queue(), task);

	run_queue_release_lock(task);
	spinlock_release(&task->state_lock);
	return 1;
}

void task_release(task_t *task) {
	if (!task) return;
	if (ref_count_dec(&task->ref_count) > 1) {
		return;
	}
	signal_context_destroy(&task->signal_context);
	kfree(task);
}

int add_fd(vfs_fd_t *fd, long flags) {
	if (IS_ERR(fd)) {
		return PTR2ERR(fd);
	}
	int interrupt_save;
	rwlock_acquire_write(&get_current_proc()->fd_table.lock, &interrupt_save);

	int index = 0;
	while (get_current_proc()->fd_table.fds[index].present) {
		index++;
		if (index >= MAX_FD) {
			// to many fds open
			return -ENXIO;
		}
	}

	file_descriptor_t *fd_data = &get_current_proc()->fd_table.fds[index];
	fd_data->present           = 1;
	fd_data->fd                = fd;
	fd_data->flags             = flags;

	rwlock_release_write(&get_current_proc()->fd_table.lock, &interrupt_save);
	return index;
}


int get_fd(int fd, file_descriptor_t *file_descriptor) {
	if (fd < 0 || fd >= MAX_FD) {
		return -EBADF;
	}

	int interrupt_save;
	rwlock_acquire_read(&get_current_proc()->fd_table.lock, &interrupt_save);

	file_descriptor_t *fd_data = &get_current_proc()->fd_table.fds[fd];
	if (!fd_data->present) {
		rwlock_release_read(&get_current_proc()->fd_table.lock, &interrupt_save);
		return -EBADF;
	}
	if (file_descriptor) *file_descriptor = *fd_data;

	rwlock_release_read(&get_current_proc()->fd_table.lock, &interrupt_save);
	return fd;
}


int close_fd(int fd) {
	if (fd < 0 || fd >= MAX_FD) {
		return -EBADF;
	}

	int interrupt_save;
	rwlock_acquire_write(&get_current_proc()->fd_table.lock, &interrupt_save);


	file_descriptor_t *fd_data = &get_current_proc()->fd_table.fds[fd];
	if (!fd_data->present) {
		rwlock_release_write(&get_current_proc()->fd_table.lock, &interrupt_save);
		return -EBADF;
	}
	vfs_close(fd_data->fd);
	fd_data->present = 0;

	rwlock_release_write(&get_current_proc()->fd_table.lock, &interrupt_save);
	return fd;
}
