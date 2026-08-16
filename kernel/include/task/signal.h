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
} signal_pending_t;

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

void signal_context_destroy(signal_context_t *signal_context);
void signal_handle(registers_t *context);

/**
 * @brief dequeue a pending signal present in mask
 * @param mask the mask of possible signals to dequeue
 * @param siginfo where to store the siginfo of the dequeued signal (optional)
 * @return the signal number if handled or 0 if no signal are pending or -EINTR if a pending signal is not present in mask
 */
int signal_dequeue(sigset_t mask, siginfo_t *siginfo);

sigset_t signal_get_pending_mask(void);
sigset_t signal_get_unhandled_mask(void);

void signal_restore_handler(registers_t *registers);

#define MAGIC_SIGRETURN 0x4848

#endif
