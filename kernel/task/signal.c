#include <kernel/userspace.h>
#include <kernel/signal.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <signal.h>
#include <ucontext.h>
#include <kernel/string.h>

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

	proc->pending_sig |= sigmask(signum);

	//from here we don't the state of the task to change during this procedure
	kernel->can_task_switch = 0;

	

	kernel->can_task_switch = 1;

	return 0;
}

void handle_signal(fault_frame *context){
	if(get_current_proc()->pending_sig & ~get_current_proc()->sig_mask){
		for (int signum = 1; signum < NSIG; signum++){
			if((get_current_proc()->pending_sig & sigmask(signum)) && !(get_current_proc()->sig_mask & sigmask(signum))){
				kdebugf("signal %d recived\n",signum);
				get_current_proc()->pending_sig &= ~sigmask(signum);
				if(get_current_proc()->sig_handling[signum].sa_handler == SIG_IGN){
					continue;
				} else if(get_current_proc()->sig_handling[signum].sa_handler){
					//this is the tricky part

					uintptr_t sp = SP_REG(*context);
					kdebugf("sp : %p\n",sp);

					//we first need make the ucontext on the userspace stack
					sp -= sizeof(ucontext_t);
					ucontext_t *ucontext = (ucontext_t *)sp;
					memset(ucontext,0,sizeof(ucontext_t));
					memcpy(&ucontext->uc_mcontext,context,sizeof(fault_frame));
					ucontext->uc_sigmask = get_current_proc()->sig_mask;

					//push the magic return value
					sp -= sizeof(uintptr_t);
					*(uintptr_t *)sp = MAGIC_SIGRETURN;

					//apply the new mask
					get_current_proc()->sig_mask |= get_current_proc()->sig_handling[signum].sa_mask;

					//then we can jump to the signal handler
					jump_userspace((void *)get_current_proc()->sig_handling[signum].sa_handler,(void *)sp,signum,0,(uintptr_t)ucontext,0);
				} else {
					switch(default_handling[signum]){
					case CORE:
					case KILL:
						kdebugf("process killed by signal %d\n",signum);
						get_current_proc()->exit_status = ((uint64_t)1 << 33) | signum;
						kill_proc(get_current_proc());
						break;
					case IGN:
						continue;
					case STOP:
						kdebugf("process should have stoped\n");
						continue;
					}
				}
			}
		}		
	}
}

void restore_signal_handler(fault_frame *context){
	kdebugf("restore signal handler\n");
	
	//since the magic return address as been poped,
	//this mean there only the ucontext left
	ucontext_t *ucontext = (ucontext_t *)SP_REG(*context);

	//restore the old mask
	get_current_proc()->sig_mask = ucontext->uc_sigmask;

	fault_frame *old_context = (fault_frame *)&ucontext->uc_mcontext;
	kdebugf("sp : %p\n",SP_REG(*old_context));

	load_context((fault_frame *)&ucontext->uc_mcontext);
}