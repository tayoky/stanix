#ifndef PROCESS_H
#define PROCESS_H

#include <kernel/paging.h>
#include <kernel/list.h>
#include <kernel/vfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <stdint.h>
#include <fcntl.h>

struct fault_frame;
struct process_struct;

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

typedef struct process_struct {
	addrspace_t addrspace;
	uintptr_t rsp;
	uintptr_t kernel_stack;
	struct fault_frame *syscall_frame;
	pid_t pid;
	struct process_struct *snext; //next field used in sleep
	struct process_struct *next;
	struct process_struct *prev;
	struct process_struct *parent;
	uint64_t flags;
	file_descriptor fds[MAX_FD];
	vfs_node *cwd_node; //TODO use this instead of making an asbolut path require vfs_openat
	char *cwd_path;
	uintptr_t heap_start;
	uintptr_t heap_end;
	struct timeval wakeup_time;
	list *memseg;
	pid_t waitfor;
	long exit_status;
	list *child;
	sigset_t sig_mask;
	sigset_t pending_sig;
	struct sigaction sig_handling[32];
	pid_t group;
	pid_t sid;
	uid_t uid;
	uid_t euid;
	uid_t suid;
	gid_t gid;
	gid_t egid;
	gid_t sgid;
	mode_t umask;
	struct process_struct *waker; //the proc that wake up us
} process;

#define PROC_FLAG_PRESENT 0x001UL
#define PROC_FLAG_ZOMBIE  0x002UL
#define PROC_FLAG_TOCLEAN 0x004UL
#define PROC_FLAG_DEAD    0x008UL
#define PROC_FLAG_RUN     0x010UL
#define PROC_FLAG_SLEEP   0x020UL
#define PROC_FLAG_WAIT    0x040UL
#define PROC_FLAG_INTR    0x080UL
#define PROC_FLAG_BLOCKED 0x100UL
#define PROC_FLAG_KERNEL  0x200UL

void init_task(void);
process *get_current_proc(void);
process *new_proc(void);
process *new_kernel_task(void (*func)(uint64_t,char**),uint64_t argc,char *argv[]);
void kill_proc(process *proc);
void proc_push(process *proc,void *value,size_t size);
process *pid2proc(pid_t pid);
int block_proc();
void unblock_proc(process *proc);

void yeld();
int is_userspace(process *);

#define FD_GET(fd) get_current_proc()->fds[fd]
#define is_valid_fd(fd)  (fd >= 0 && fd < MAX_FD && (FD_GET(fd).present))
#define FD_CHECK(fd,flag) (FD_GET(fd).flags & flag)

extern list *proc_list;
extern process *sleeping_proc;

#define EUID_ROOT 0

#endif