#include <kernel/arch.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/signal.h>
#include <kernel/string.h>
#include <kernel/userspace.h>
#include <kernel/xarray.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <ucontext.h>

#define IGN  0
#define KILL 1
#define CORE 2
#define STOP 3
#define CONT 4

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

static void handle_default(int signum) {
	switch (default_handling[signum]) {
	case CORE:
	case KILL:
		spinlock_release(&get_current_task()->sig_lock);
		kdebugf("proc killed by signal %d\n", signum);
		proc_exit((1U << 17) | signum);
		break;
	case IGN:
	case CONT:
		break;
	case STOP:
		// FIXME : full of RACE CONDITION
		// TODO : stop whole process
		spinlock_release(&get_current_task()->sig_lock);
		kdebugf("task stopped\n");
		int ret = 0;
		// FIXME : i'm pretty sure if main thread recive SIGSTOP the whole process should stop
		// block until recive a continue signals or kill
		set_task_status(TASK_STATUS_STOPPED);
		while ((ret = block_task())) {
			if (ret != EINTR) {
				// uh
				kdebugf("signal bug\n");
				get_current_task()->status = TASK_STATUS_RUNNING;
				return;
			}
			for (int i = 0; i < NSIG; i++) {
				spinlock_acquire(&get_current_task()->sig_lock);
				if ((sigmask(i) & get_current_task()->pending_sig) && !(sigmask(i) & get_current_task()->sig_mask)
					&& get_current_task()->sig_handling[i].sa_handler == SIG_DFL && default_handling[i] != IGN) {
					if (default_handling[i] == STOP) {
						// ignore other stop
						get_current_task()->pending_sig &= ~sigmask(i);
						break;
					}
					spinlock_release(&get_current_task()->sig_lock);
					get_current_task()->status = TASK_STATUS_RUNNING;
					return;
				}
				spinlock_release(&get_current_task()->sig_lock);
				set_task_status(TASK_STATUS_STOPPED);
			}
		}
		break;
	}
}

int signal_send_group(process_group_t *group, int signum) {
	if (!group) return -ESRCH;
	rculist_foreach (node, group) {
		process_t *proc = container_of(node, process_t, group_node);
		signal_send(proc, signum);
	}
	return 0;
}

int signal_send(process_t *proc, int signum) {
	return signal_send_task(proc->main_thread, signum);
}

int signal_send_task(task_t *thread, int signum) {
	kdebugf("send %d to %ld\n", signum, thread->tid);

	spinlock_acquire(&thread->sig_lock);
	// if the process ignore just skip
	if (thread->sig_handling[signum].sa_handler == SIG_IGN || (thread->sig_handling[signum].sa_handler == SIG_DFL && default_handling[signum] == IGN)) {
		spinlock_release(&thread->sig_lock);
		return 0;
	}

	thread->pending_sig |= sigmask(signum);

	// if the sig is blocked don't handle
	if (thread->sig_mask & sigmask(signum)) {
		spinlock_release(&thread->sig_lock);
		return 0;
	}

	// if the task is blocked interrupt it
	unblock_task_reason(thread, WAKEUP_SIGNAL);

	spinlock_release(&thread->sig_lock);
	return 0;
}

void handle_signal(registers_t *context) {
	spinlock_acquire(&get_current_task()->sig_lock);
	sigset_t to_handle = get_current_task()->pending_sig & ~get_current_task()->sig_mask;

	// nothing to handle ? just return
	if (!to_handle) {
		spinlock_release(&get_current_task()->sig_lock);
		return;
	}

	for (int signum = 1; signum < NSIG; signum++) {
		if (to_handle & sigmask(signum)) {
			kdebugf("signal %d recived\n", signum);

			// clear the pending bit
			get_current_task()->pending_sig &= ~sigmask(signum);

			if (get_current_task()->sig_handling[signum].sa_handler == SIG_IGN) {
				continue;
			} else if (get_current_task()->sig_handling[signum].sa_handler == SIG_DFL) {
				handle_default(signum);
				continue;
			} else {
				spinlock_release(&get_current_task()->sig_lock);
				// TODO : move this to arch specific
				// this is the tricky part
				uintptr_t sp = SP_REG(*context);
				kdebugf("sp : %p\n", sp);

				// jump the red zone
				sp -= 128;

				// align the stack
				sp &= ~0xfUL;

				// we need make the ucontext on the userspace stack
				sp -= sizeof(ucontext_t);
				// UNSAFE
				ucontext_t *ucontext = (ucontext_t *)sp;
				memset(ucontext, 0, sizeof(ucontext_t));
				ucontext->uc_sigmask = get_current_task()->sig_mask;

				// save machine context
				acontext_t *saved_context = (acontext_t *)&ucontext->uc_mcontext;
				arch_save_context(saved_context);
				saved_context->frame = *context;

				// push the magic return value
				sp -= sizeof(uintptr_t);
				*(uintptr_t *)sp = MAGIC_SIGRETURN;
				// apply the new mask
				get_current_task()->sig_mask |= get_current_task()->sig_handling[signum].sa_mask;
				// then we can jump to the signal handler
				jump_userspace((void *)get_current_task()->sig_handling[signum].sa_handler, (void *)sp, signum, 0, (uintptr_t)ucontext, 0);
			}
		}
	}
	spinlock_release(&get_current_task()->sig_lock);
}

void restore_signal_handler(registers_t *context) {
	kdebugf("restore signal handler\n");

	// since the magic return address as been poped,
	// this mean there only the ucontext left
	ucontext_t *ucontext = (ucontext_t *)SP_REG(*context);

	// restore the old mask
	spinlock_acquire(&get_current_task()->sig_lock);
	get_current_task()->sig_mask = ucontext->uc_sigmask;
	spinlock_release(&get_current_task()->sig_lock);

	acontext_t *old_context = (acontext_t *)&ucontext->uc_mcontext;
	kdebugf("sp : %p\n", SP_REG(old_context->frame));

	arch_load_context((acontext_t *)&ucontext->uc_mcontext);
}
