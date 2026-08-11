#include <kernel/arch.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/signal.h>
#include <kernel/string.h>
#include <kernel/userspace.h>
#include <kernel/slab.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <ucontext.h>

#define IGN  0
#define KILL 1
#define CORE 2
#define STOP 3
#define CONT 4

static slab_cache_t signal_pendings_slab;

static const char default_handling[] = {
	[SIGHUP]    = KILL,
	[SIGINT]    = KILL,
	[SIGQUIT]   = CORE,
	[SIGILL]    = CORE,
	[SIGTRAP]   = CORE,
	[SIGABRT]   = CORE,
	[SIGCAT]    = IGN,
	[SIGFPE]    = CORE,
	[SIGKILL]   = KILL,
	[SIGBUS]    = CORE,
	[SIGSEGV]   = CORE,
	[SIGSYS]    = CORE,
	[SIGPIPE]   = KILL,
	[SIGALRM]   = KILL,
	[SIGTERM]   = KILL,
	[SIGURG]    = IGN,
	[SIGSTOP]   = STOP,
	[SIGTSTP]   = STOP,
	[SIGCONT]   = CONT,
	[SIGCHLD]   = IGN,
	[SIGTTIN]   = STOP,
	[SIGTTOU]   = STOP,
	[SIGPOLL]   = KILL,
	[SIGXCPU]   = CORE,
	[SIGXFSZ]   = CORE,
	[SIGVTALRM] = KILL,
	[SIGPROF]   = KILL,
	[SIGWINCH]  = IGN,
	[SIGINFO]   = IGN,
	[SIGUSR1]   = IGN,
	[SIGUSR2]   = IGN,
	[SIGTHR]    = IGN,
};

void init_signal(void) {
	slab_init(&signal_pendings_slab, sizeof(signal_pending_t), "signal-pendings");
}

static signal_pending_t *signal_create_pending(siginfo_t *siginfo) {
	signal_pending_t *signal_pending = slab_alloc(&signal_pendings_slab);
	if (!signal_pending) return NULL;
	signal_pending->siginfo = *siginfo;
	signal_pending->siginfo.si_pid = get_current_proc()->pid;
	signal_pending->siginfo.si_uid = get_current_uid();
	return signal_pending;
}

static int signal_add_pending(signal_context_t *signal_context, process_t *proc, signal_pending_t *signal_pending) {
	spinlock_assert_acquired(&signal_context->lock);

	int signum = signal_pending->siginfo.si_signo;

	if (signal_context->pending_mask & sigmask(signum)) {
		// already pending
		return 0;
	}
	
	spinlock_acquire(&proc->proc_lock);
	if (proc->sig_handlers[signum].sa_handler == SIG_IGN || (proc->sig_handlers[signum].sa_handler == SIG_DFL && default_handling[signum] == IGN)) {
		spinlock_release(&proc->proc_lock);
		// ignored
		return 0;
	}
	spinlock_release(&proc->proc_lock);

	list_append(&signal_context->pendings, &signal_pending->node);
	signal_context->pending_mask |= sigmask(signum);
	return 1;
}

int signal_send_siginfo_group(process_group_t *group, siginfo_t *siginfo) {
	if (!group) return -ESRCH;
	rculist_foreach (node, &group->processes) {
		process_t *proc = container_of(node, process_t, group_node);
		signal_send_siginfo_proc(proc, siginfo);
	}
	return 0;
}

int signal_send_siginfo_proc(process_t *proc, siginfo_t *siginfo) {
	kdebugf("send %d to process %ld\n", siginfo->si_signo, proc->pid);

	signal_pending_t *signal_pending = signal_create_pending(siginfo);
	if (!signal_pending) return -ENOMEM;

	spinlock_acquire(&proc->signal_context.lock);
	if (signal_add_pending(&proc->signal_context, proc, signal_pending)) {
		spinlock_acquire(&proc->proc_lock);
		// TODO : interrupt a single task instead of every task in the process
		foreach (node, &proc->threads) {
			task_t *task = container_of(node, task_t, thread_list_node);
			unblock_task_reason(task, WAKEUP_SIGNAL);
		}
		spinlock_release(&proc->proc_lock);
		spinlock_release(&proc->signal_context.lock);
	} else {
		spinlock_release(&proc->signal_context.lock);
		slab_free(signal_pending);
	}

	return 0;
}

int signal_send_siginfo_task(task_t *thread, siginfo_t *siginfo) {
	kdebugf("send %d to task %ld\n", siginfo->si_signo, thread->tid);

	signal_pending_t *signal_pending = signal_create_pending(siginfo);
	if (!signal_pending) return -ENOMEM;

	spinlock_acquire(&thread->signal_context.lock);
	if (signal_add_pending(&thread->signal_context, thread->process, signal_pending)) {
		// if the sig is unblocked interrupt
		if (thread->sig_mask & sigmask(siginfo->si_signo)) {
			unblock_task_reason(thread, WAKEUP_SIGNAL);
			return 0;
		}
		spinlock_release(&thread->signal_context.lock);
	} else {
		spinlock_release(&thread->signal_context.lock);
		slab_free(signal_pending);
	}

	return 0;
}

void signal_context_destroy(signal_context_t *signal_context) {
	spinlock_acquire(&signal_context->lock);
	list_node_t *node = signal_context->pendings.first_node;
	while (node) {
		signal_pending_t *signal = container_of(node, signal_pending_t, node);
		node = node->next;
		slab_free(signal);
	}
}

static inline void proc_sigexit(int signum) {
	kdebugf("proc killed by signal %d\n", signum);
	proc_exit((1U << 17) | signum);
}

static void handle_default(int signum) {
	switch (default_handling[signum]) {
	case CORE:
	case KILL:
		proc_sigexit(signum);
		break;
	case IGN:
	case CONT:
		break;
	case STOP:
		// TODO
		kwarningf("TODO : implement process stoping\n");
		break;
	}
}

typedef struct signal_frame {
	ucontext_t ucontext;
	siginfo_t siginfo;
} signal_frame_t;

static void signal_handle_siginfo(siginfo_t *siginfo, registers_t *registers) {
	kdebugf("signal %d recived\n", siginfo->si_signo);

	spinlock_acquire(&get_current_task()->signal_context.lock);
	spinlock_acquire(&get_current_proc()->proc_lock);
	struct sigaction handler = get_current_proc()->sig_handlers[siginfo->si_signo];
				
	if ((handler.sa_flags & SA_RESETHAND) && handler.sa_handler != SIG_IGN) {
		get_current_proc()->sig_handlers[siginfo->si_signo].sa_handler = SIG_DFL;
	}
	spinlock_release(&get_current_proc()->proc_lock);
	spinlock_release(&get_current_task()->signal_context.lock);

	if (handler.sa_handler == SIG_IGN) {
		return;
	} else if (handler.sa_handler == SIG_DFL) {
		handle_default(siginfo->si_signo);
		return;
	} else {
		// TODO : move this to arch specific
		// this is the tricky part
		uintptr_t sp = SP_REG(*registers);
		kdebugf("sp : %p\n", sp);
		
		// jump the red zone
		sp -= 128;

		// we need make place for the signal frame on the userspace stack
		sp -= sizeof(signal_frame_t);

		// align the stack
		sp &= ~0xfUL;
	
		// setup frame
		signal_frame_t frame = {0};
		frame.ucontext.uc_sigmask = get_current_task()->sig_mask;
		acontext_t *saved_context = (acontext_t *)&frame.ucontext.uc_mcontext;
		arch_save_context(saved_context);
		saved_context->frame = *registers;
		frame.siginfo = *siginfo;
	
		// push to userspace
		signal_frame_t *user_frame = (signal_frame_t *)sp;
		if (user_copy_auto_to(user_frame, &frame) < 0) {
			// avoid infinite recursion shit that could happen if we just send SIGSEGV
			proc_sigexit(SIGILL);
		}
		*user_frame = frame;

		// push the magic return value
		sp -= sizeof(uintptr_t);
		uintptr_t magic_return = MAGIC_SIGRETURN;
		if (user_copy_auto_to((uintptr_t*)sp, &magic_return) < 0) {
			// avoid infinite recursion shit that could happen if we just send SIGSEGV
			proc_sigexit(SIGILL);
		}

		// apply the new mask
		spinlock_acquire(&get_current_task()->signal_context.lock);
		get_current_task()->sig_mask |= handler.sa_mask;
		if (!(handler.sa_flags & SA_NODEFER)) {
			get_current_task()->sig_mask |= sigmask(siginfo->si_signo);
		}
		spinlock_release(&get_current_task()->signal_context.lock);

		// then we can setup the args for the signal handler
		SP_REG(*registers)   = sp;
		PC_REG(*registers)   = (uintptr_t)handler.sa_handler;
		ARG1_REG(*registers) = siginfo->si_signo;
		ARG2_REG(*registers) = (uintptr_t)&user_frame->siginfo;
		ARG3_REG(*registers) = (uintptr_t)&user_frame->ucontext;
	}
}

static void signal_handle_context(signal_context_t *signal_context, registers_t *registers) {
	spinlock_acquire(&signal_context->lock);

	list_node_t *node = signal_context->pendings.first_node;
	while (node) {
		signal_pending_t *signal = container_of(node, signal_pending_t, node);
		node = node->next;
		if (sigmask(signal->siginfo.si_signo) & get_current_task()->sig_mask) {
			// signal is blocked
			continue;
		}
		list_remove(&signal_context->pendings, &signal->node);
		
		// clear the pending bit
		signal_context->pending_mask &= ~sigmask(signal->siginfo.si_signo);

		spinlock_release(&signal_context->lock);

		siginfo_t siginfo = signal->siginfo;
		slab_free(signal);
		signal_handle_siginfo(&siginfo, registers);

		spinlock_acquire(&signal_context->lock);
	}
	spinlock_release(&signal_context->lock);
}

void signal_handle(registers_t *registers) {
	// handle thread wide signals
	signal_handle_context(&get_current_task()->signal_context, registers);

	// handle process wide signals
	signal_handle_context(&get_current_proc()->signal_context, registers);
}

void restore_signal_handler(registers_t *context) {
	kdebugf("restore signal handler\n");

	// since the magic return address as been poped,
	// this mean there only the ucontext left
	ucontext_t *ucontext = (ucontext_t *)SP_REG(*context);

	// restore the old mask
	spinlock_acquire(&get_current_task()->signal_context.lock);
	get_current_task()->sig_mask = ucontext->uc_sigmask;
	spinlock_release(&get_current_task()->signal_context.lock);

	acontext_t *old_context = (acontext_t *)&ucontext->uc_mcontext;
	kdebugf("sp : %p\n", SP_REG(old_context->frame));

	arch_load_context((acontext_t *)&ucontext->uc_mcontext);
}
