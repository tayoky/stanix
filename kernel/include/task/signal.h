#ifndef KERNEL_SIGNAL_H
#define KERNEL_SIGNAL_H

#include <kernel/arch.h>
#include <kernel/list.h>
#include <abi/signal.h>

struct task;
struct process;
struct process_group;

typedef struct signal_pending {
	list_node_t node;
	siginfo_t siginfo;
} signal_pending;

typedef struct signal_context {
	list_t pendings;       // protected by lock
	sigset_t pending_mask; // protected by lock
	spinlock_t lock;
} signal_context_t;

void init_signal(void);
int signal_send_siginfo_task(struct task *task, siginfo_t *siginfo);
int signal_send_siginfo_proc(struct process *proc, siginfo_t *siginfo);
int signal_send_siginfo_group(struct process_group *group, siginfo_t *siginfo);

static inline int signal_send_task(struct task *task, int signum) {
	siginfo_t siginfo = {
		.si_signo = signum,
	};
	return signal_send_siginfo_task(task, &siginfo);
}

static inline int signal_send_proc(struct process *proc, int signum) {
	siginfo_t siginfo = {
		.si_signo = signum,
	};
	return signal_send_siginfo_proc(proc, &siginfo);
}

static inline int signal_send_group(struct process_group *group, int signum) {
	siginfo_t siginfo = {
		.si_signo = signum,
	};
	return signal_send_siginfo_group(group, &siginfo);
}

void signal_handle(registers_t *context);
void restore_signal_handler(registers_t *context);

#define MAGIC_SIGRETURN 0x4848

#endif
