#ifndef PROCESS_H
#define PROCESS_H

#include <kernel/arch.h>
#include <kernel/paging.h>
#include <kernel/list.h>
#include <kernel/vfs.h>
#include <kernel/mutex.h>
#include <kernel/spinlock.h>
#include <kernel/sleep.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <stdint.h>
#include <fcntl.h>

struct fault_frame;
struct process;

#define MAX_FD 32

typedef struct {
	vfs_fd_t *fd;
	size_t offset;
	long present;
	long flags;
} file_descriptor;

struct process;

typedef struct task {
	list_node_t task_list_node;
	list_node_t thread_list_node;
	acontext context;
	struct process *process;
	struct task *snext;
	struct task *sprev;
	struct task *next;
	struct task *prev;
	pid_t tid;
	sigset_t sig_mask;
	mutex_t sig_lock;
	sigset_t pending_sig;
	struct sigaction sig_handling[32];

	struct timeval wakeup_time;
	pid_t waitfor;
	atomic_int flags;
	spinlock state_lock;
	uintptr_t kernel_stack;

	struct fault_frame *syscall_frame;
	void *exit_arg;
	struct task *waker;
	struct task * _Atomic waiter; //task waiting on us
} task_t;

typedef struct process {
	list_node_t proc_list_node;
	list_node_t child_list_node;
	addrspace_t addrspace;
	pid_t pid;
	struct process *parent;
	spinlock state_lock;
	file_descriptor fds[MAX_FD];
	vfs_node_t *cwd_node;
	char *cwd_path;
	uintptr_t heap_start;
	uintptr_t heap_end;
	list_t memseg;
	list_t child;
	list_t threads;
	pid_t group;
	pid_t sid;
	uid_t uid;
	uid_t euid;
	uid_t suid;
	gid_t gid;
	gid_t egid;
	gid_t sgid;
	mode_t umask;
	task_t *main_thread;
	long exit_status;
} process_t;

#define TASK_FLAG_PRESENT 0x01
#define TASK_FLAG_ZOMBIE  0x02
#define TASK_FLAG_RUN     0x04 //is the task actually running on a cpu
#define TASK_FLAG_WAIT    0x08 //is the task blocked ?
#define TASK_FLAG_INTR    0x10
#define TASK_FLAG_BLOCKED 0x20
#define TASK_FLAG_STOPPED 0x40
#define TASK_FLAG_SLEEP   0x80

void init_task(void);
process_t *get_current_proc(void);
task_t *get_current_task(void);
process_t *new_proc(void);
task_t *new_task(process_t *proc);
process_t *new_kernel_task(void (*func)(uint64_t,char**),uint64_t argc,char *argv[]);

/// @brief kill the current thread
void kill_task(void);

/// @brief kill the current process
void kill_proc();

void final_proc_cleanup(process_t *proc);


void final_task_cleanup(task_t *thread);

/// @brief get a process from its pid
/// @param pid the pid of the process
/// @return the process with the specfied pid
process_t *pid2proc(pid_t pid);

task_t *tid2task(pid_t tid);

/// @brief block the current proc
/// @return -EINTR if intruppted by signal devlivery or 0
int block_task(void);

/// @brief unblock a task
/// @param proc the task to unblock
void unblock_task(task_t *thread);

/// @brief yield to next task
/// @param addback do we add the task back to the queue or running task
void yield(int addback);

/// @brief wait for any thread in a group of threads to die
/// @param threads the threads list
/// @param threads_count the number of threads in the threads list
/// @param flags flags such as WNOHANG
/// @param waker the thread that died
/// @return the tid of the threads that died or negative errno number
int waitfor(task_t **threads,size_t threads_count,int flags,task_t **waker);


int add_fd(vfs_fd_t *fd);

#define FD_GET(fd) get_current_proc()->fds[fd]
#define is_valid_fd(fd)  (fd >= 0 && fd < MAX_FD && (FD_GET(fd).present))
#define FD_CHECK(fd,flag) (FD_GET(fd).flags & flag)

extern list_t proc_list;
extern task_t *sleeping_proc;
extern spinlock sleep_lock;

#define EUID_ROOT 0
#define KSTACK_TOP(kstack) (((kstack) + KERNEL_STACK_SIZE) & ~0xFUL)

#endif
