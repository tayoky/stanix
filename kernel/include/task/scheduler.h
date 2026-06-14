#ifndef KERNEL_SCHEDULER_H
#define KERNEL_SCHEDULER_H

#include <kernel/arch.h>
#include <kernel/list.h>
#include <kernel/mmu.h>
#include <kernel/refcount.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>

struct registers;
struct process;
struct task;

typedef struct run_queue {
	spinlock_t lock;
	list_t tasks;
	struct task *prev;
	struct task *current;
	int prev_is_on_queue;
} run_queue_t;

typedef struct task {
	list_node_t thread_list_node;
	list_node_t waiter_list_node;
	list_node_t run_list_node;
	acontext_t context;
	ref_count_t ref_count;
	struct process *process;
	pid_t tid;
	sigset_t sig_mask;
	spinlock_t sig_lock;
	sigset_t pending_sig;
	struct sigaction sig_handling[32];

	struct timespec wakeup_time;
	pid_t waitfor;
	atomic_int flags;
	int status;
	int wakeup_reason;
	uintptr_t kernel_stack;

	struct registers *syscall_frame;
	void *exit_arg;
	struct task *waker;
	struct task *_Atomic waiter; // task waiting on us
	spinlock_t state_lock;
	run_queue_t *_Atomic run_queue;
	size_t preempt_disable;
	atomic_size_t preempt_context_switches;
	atomic_size_t voluntary_context_switches;
} task_t;

#define TASK_STATUS_RUNNING       1 // is the task actually running on a cpu
#define TASK_STATUS_ZOMBIE        2
#define TASK_STATUS_BLOCKED       3 // is the task blocked ?
#define TASK_STATUS_INTERRUPTIBLE 4 // is the task blocked and interruptible ?
#define TASK_STATUS_STOPPED       5

#define TASK_FLAG_WAIT  0x02
#define TASK_FLAG_SLEEP 0x04

#define WAKEUP_TIMEOUT 0
#define WAKEUP_SIGNAL  1
#define WAKEUP_OTHER   2

#define EUID_ROOT 0

void init_task(void);

task_t *get_current_task(void);

task_t *new_kernel_task(void (*func)(void *arg), void *arg);

/**
 * @brief create a new task
 * @param proc the proc to create the task under
 * @param func the kernel function to execute
 * @param arg the argument to pass to the kernel function
 * @return a pointer to the new task
 */
task_t *new_task(struct process *proc, void (*func)(void *arg), void *arg);

/**
 * @brief kill the current task
 */
void kill_task(void);

/**
 * @brief get a task from its tid
 * @param tid the tid of the task
 * @return the task with the specfied tid
 * @note this create a new reference to the task
 */
task_t *tid2task(pid_t tid);

/**
 * @brief increment the ref count of a task
 * @param task the task to increment the ref count of
 * @return the task
 */
static inline task_t *task_ref(task_t *task) {
	if (task) ref_count_inc(&task->ref_count);
	return task;
}

/**
 * @brief release a reference to a task
 * @param task the task to release
 */
void task_release(task_t *task);

/**
 * @brief safely set the status of the current task
 * @param status the status to set it to
 */
static inline void set_task_status(int status) {
	spinlock_acquire(&get_current_task()->state_lock);
	get_current_task()->status = status;
	spinlock_release(&get_current_task()->state_lock);
}

/**
 * @brief prepare the current task to sleep
 */
static inline void block_prepare(void) {
	set_task_status(TASK_STATUS_BLOCKED);
}

/**
 * @brief prepare the current task to sleep but can be interrupted by signals
 */
static inline void block_prepare_interruptible(void) {
	set_task_status(TASK_STATUS_INTERRUPTIBLE);
}

/**
 * @brief cancel a preparation to sleep
 */
static inline void block_cancel(void) {
	set_task_status(TASK_STATUS_RUNNING);
}

/**
 * @brief enable preempt for this core, must be called the number of time \ref preempt_disable was called
 */
static inline void preempt_enable(void) {
	if (get_current_task()) get_current_task()->preempt_disable--;
}

/**
 * @brief disable preempt for this core
 */
static inline void preempt_disable(void) {
	if (get_current_task()) get_current_task()->preempt_disable++;
}

/**
 * @brief block the current task
 * @param timeout the timeout (timespec until it can block)
 * @return -EINTR if interrupted by signal delivery or -ETIMEDOUT if interrupted by timeout or 0
 */
int block_task_timeout(struct timespec *timeout);

/**
 * @brief block the current task
 * @return -EINTR if interrupted by signal delivery or 0
 */
int block_task(void);

/**
 * @brief unblock a task for a reason
 * @param task the task to unblock
 * @param reason the reason to unblock
 * @return 1 if unblocked the task else 0
 */
int unblock_task_reason(task_t *task, int reason);


/**
 * @brief unblock a task
 * @param task the task to unblock
 * @return 1 if unblocked the task else 0
 */
static inline int unblock_task(task_t *task) {
	return unblock_task_reason(task, WAKEUP_OTHER);
}

/**
 * @brief yield to next task
 * @param preempt was this yield volontary
 */
void yield(int preempt);


void finish_yield(void);

/**
 * @brief wait for any thread in a group of threads to die
 * @param threads the threads list
 * @param threads_count the number of threads in the threads list
 * @param flags flags such as WNOHANG
 * @param waker the thread that died
 * @return the tid of the threads that died or negative errno number
 */
int waitfor(task_t **threads, size_t threads_count, int flags, task_t **waker);

extern list_t sleeping_tasks;
extern spinlock_t sleep_lock;

#define KSTACK_TOP(kstack) (((kstack) + KERNEL_STACK_SIZE) & ~0xFUL)

#endif
