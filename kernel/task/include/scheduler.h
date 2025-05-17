#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <kernel/vfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <kernel/list.h>
#include <sys/signal.h>

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
#define FD_CLOEXEC  0x10

typedef struct memseg_struct {
	void *offset;
	size_t size;
	struct memseg_struct *next;
	struct memseg_struct *prev;
	uint64_t flags;
}memseg;

typedef struct process_struct{
	uint64_t cr3;            //WARNING !!!!
	uint64_t rsp;            //this first two value are reference in switch.s
	struct fault_frame *syscall_frame; //any change here must be also done in this file
	pid_t pid;
	struct process_struct *snext; //next field used in sleep
	struct process_struct *next;
	struct process_struct *prev;
	struct process_struct *parent;
	uint64_t flags;
	file_descriptor fds[MAX_FD];
	vfs_node *cwd_node; //TODO use this instead of making an asbolut path require vfs_openat
	char *cwd_path;
	uint64_t heap_start;
	uint64_t heap_end;
	struct timeval wakeup_time;
	memseg *first_memseg;
	pid_t waitfor;
	long exit_status;
	list *child;
	sigset_t sig_mask;
} process;

#define PROC_STATE_PRESENT 0x01
#define PROC_STATE_ZOMBIE  0x02
#define PROC_STATE_TOCLEAN 0x04
#define PROC_STATE_DEAD    0x08
#define PROC_STATE_RUN     0x10
#define PROC_STATE_SLEEP   0x20
#define PROC_STATE_WAIT    0x40

void init_task();
process *get_current_proc();
process *new_proc();
process *new_kernel_task(void (*func)(uint64_t,char**),uint64_t argc,char *argv[]);
void kill_proc(process *proc);
void proc_push(process *proc,void *value,size_t size);
process *pid2proc(pid_t pid);
void block_proc();
void unblock_proc(process *proc);

void yeld();

#define FD_GET(fd) get_current_proc()->fds[fd]
#define is_valid_fd(fd)  (fd >= 0 && fd < MAX_FD && (FD_GET(fd).present))
#define FD_CHECK(fd,flag) (FD_GET(fd).flags & flag)

extern list *to_clean_proc;
extern process *cleaner;
extern process *sleeping_proc;

#endif