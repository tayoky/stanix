#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

typedef uint64_t pid_t;

struct process_struct;

typedef struct process_struct{
	uint64_t cr3;
	uint64_t rsp;
	pid_t pid;
	struct process_struct *next;
	struct process_struct *parent;
	uint64_t state;
} process;

#define PROC_STATE_PRESENT 0x01
#define PROC_STATE_DEAD    0x02
#define PROC_STATE_RUN     0x04

void init_task();
process *get_current_proc();
process *new_proc();
process *new_kernel_task(void (*func));

#endif