#include <kernel/signal.h>
#include <kernel/kernel.h>
#include <signal.h>

#define IGN  0
#define KILL 1
#define CORE 2
#define STOP 3
#define CONT 4

static const char default_handling[32] = {
	[SIGHUP]  = KILL,
	[SIGINT]  = KILL,
	[SIGQUIT] = KILL,
	[SIGILL]  = CORE,
	[SIGTRAP] = IGN,
	[SIGFPE]  = CORE,
	[SIGKILL] = KILL,
	[SIGTERM] = KILL,
	[SIGSTOP] = STOP,
	[SIGCONT] = CONT,
};

int send_sig(process *proc,int signum){
	//if the process ignore just skip
	if(proc->sig_handling[signum].sa_handler == SIG_IGN || (proc->sig_handling[signum].sa_handler == SIG_DFL && default_handling[signum] == IGN)){
		return 0;
	}

	//if blocked the signal become pending
	if(proc->sig_mask & sigmask(signum)){
		proc->pending_sig |= sigmask(signum);
		return 0;

	}

	//from here we don't the state of the task to change during this procedure
	kernel->can_task_switch = 0;

	

	kernel->can_task_switch = 1;

	return 0;
}
