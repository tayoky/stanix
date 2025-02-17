#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include "vfs.h"
#include <sys/time.h>

typedef uint64_t pid_t;

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

typedef struct process_struct{
	uint64_t cr3;
	uint64_t rsp;
	pid_t pid;
	struct process_struct *next;
	struct process_struct *parent;
	uint64_t flags;
	file_descriptor fds[MAX_FD];
	file_descriptor cwd;
	uint64_t heap_start;
	uint64_t heap_end;
	struct timeval wakeup_time;
} process;

#define PROC_STATE_PRESENT 0x01
#define PROC_STATE_DEAD    0x02
#define PROC_STATE_RUN     0x04
#define PROC_STATE_SLEEP   0x08

void init_task();
process *get_current_proc();
process *new_proc();
process *new_kernel_task(void (*func)(uint64_t,char**),uint64_t argc,char *argv[]);
void kill_proc(process *proc);
void proc_push(process *proc,uint64_t value);

void yeld();

#define is_valid_fd(fd)  (fd >= 0 && (get_current_proc()->fds[fd].present) && fd < MAX_FD)
#define FD_CHECK(fd,flag) (get_current_proc()->fds[fd].flags & flag)

#endif