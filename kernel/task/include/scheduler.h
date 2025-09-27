#ifndef PROCESS_H
#define PROCESS_H

#include <kernel/arch.h>
#include <kernel/paging.h>
#include <kernel/list.h>
#include <kernel/vfs.h>
#include <kernel/mutex.h>
#include <kernel/spinlock.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <stdint.h>
#include <fcntl.h>

struct fault_frame;
struct process;

#define MAX_FD 32

typedef struct {
	vfs_node *node;
	uint64_t offset;
	uint64_t present;
	uint64_t flags;
}file_descriptor;

#define FD_READ     0x01
#define FD_WRITE    0x02
#define FD_APPEND   0x04
#define FD_NONBLOCK 0x08

struct process;

typedef struct task {
	acontext context;
	struct process *process;
	struct task *snext;
	struct task *next;
	struct task *prev;
	pid_t tid;
	sigset_t sig_mask;
	mutex_t sig_lock;
	sigset_t pending_sig;
	struct sigaction sig_handling[32];

	struct timeval wakeup_time;
	pid_t waitfor;
	long exit_status;
	atomic_int flags;
	spinlock state_lock;
	uintptr_t kernel_stack;

	struct fault_frame *syscall_frame;
} task;

typedef struct process {
	addrspace_t addrspace;
	pid_t pid;
	struct process *parent;
	spinlock state_lock;
	file_descriptor fds[MAX_FD];
	vfs_node *cwd_node;
	char *cwd_path;
	uintptr_t heap_start;
	uintptr_t heap_end;
	list *memseg;
	list *child;
	pid_t group;
	pid_t sid;
	uid_t uid;
	uid_t euid;
	uid_t suid;
	gid_t gid;
	gid_t egid;
	gid_t sgid;
	mode_t umask;
	struct process *waker; //the proc that wake up us
	task *main_thread;
} process;

#define PROC_FLAG_PRESENT 0x01
#define PROC_FLAG_ZOMBIE  0x02
#define PROC_FLAG_RUN     0x04 //is the task actually running on a cpu
#define PROC_FLAG_WAIT    0x08 //is the task blocked ?
#define PROC_FLAG_INTR    0x10
#define PROC_FLAG_BLOCKED 0x20
#define PROC_FLAG_STOPPED 0x40

void init_task(void);
process *get_current_proc(void);
task *get_current_task(void);
process *new_proc(void);
task *new_task(process *proc);
process *new_kernel_task(void (*func)(uint64_t,char**),uint64_t argc,char *argv[]);

/// @brief kill the current thread
void kill_task(void);

void final_proc_cleanup(process *proc);

/// @brief get a process from its pid
/// @param pid the pid of the process
/// @return the process with the specfied pid
process *pid2proc(pid_t pid);

/// @brief block the current proc
/// @return -EINTR if intruppted by signal devlivery or 0
int block_task(void);

/// @brief unblock a task
/// @param proc the task to unblock
void unblock_task(task *thread);

/// @brief yield to next task
/// @param addback do we add the task back to the queue or running task
void yield(int addback);

#define FD_GET(fd) get_current_proc()->fds[fd]
#define is_valid_fd(fd)  (fd >= 0 && fd < MAX_FD && (FD_GET(fd).present))
#define FD_CHECK(fd,flag) (FD_GET(fd).flags & flag)

extern list *proc_list;
extern task *sleeping_proc;

#define EUID_ROOT 0
#define KSTACK_TOP(kstack) (((kstack) + KERNEL_STACK_SIZE) & ~0xFUL)

//TODO : make a kill_proc
#define kill_proc kill_task

#endif
