#include <kernel/userspace.h>
#include <kernel/signal.h>
#include <kernel/kernel.h>
#include <kernel/print.h>
#include <kernel/string.h>
#include <stdatomic.h>
#include <ucontext.h>
#include <signal.h>
#include <errno.h>

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

static void handle_default(int signum){
	switch(default_handling[signum]){
	case CORE:
	case KILL:
		release_mutex(&get_current_proc()->sig_lock);
		kdebugf("process killed by signal %d\n",signum);
		get_current_proc()->exit_status = ((uint64_t)1 << 17) | signum;
		kill_proc();
		break;
	case IGN:
	case CONT:
		break;
	case STOP:
		release_mutex(&get_current_proc()->sig_lock);
		kdebugf("process stopped\n");
		int ret = 0;
		//block until recive a continue signals or kill
		get_current_proc()->flags |= PROC_FLAG_STOPPED;
		while((ret = block_proc(NULL))){
			if(ret != EINTR){
				//uh
				kdebugf("signal bug\n");
				get_current_proc()->flags &= ~PROC_FLAG_STOPPED;
				return;
			}
			for(int i=0; i<NSIG; i++){
				acquire_mutex(&get_current_proc()->sig_lock);
				if((sigmask(i) & get_current_proc()->pending_sig) && !(sigmask(i) & get_current_proc()->sig_mask) 
				&& get_current_proc()->sig_handling[i].sa_handler == SIG_DFL && default_handling[i] != IGN){
					if(default_handling[i] == STOP){
						//ignore other stop
						get_current_proc()->pending_sig &= ~sigmask(i);
						break;
					}
					release_mutex(&get_current_proc()->sig_lock);
					get_current_proc()->flags &= ~PROC_FLAG_STOPPED;
					return;
				}
				release_mutex(&get_current_proc()->sig_lock);
			}
		}
		break;
	}
}

int send_sig_pgrp(pid_t pgrp,int signum){
	int ret = -1;
	foreach(node,proc_list){
		process *proc = node->value;
		if(proc->group == pgrp){
			send_sig(proc,signum);
			ret = 0;
		}
	}
	return ret;
}

int send_sig(process *proc,int signum){
	kdebugf("send %d to %ld\n",signum,proc->pid);

	acquire_mutex(&proc->sig_lock);
	//if the process ignore just skip
	if(proc->sig_handling[signum].sa_handler == SIG_IGN || (proc->sig_handling[signum].sa_handler == SIG_DFL && default_handling[signum] == IGN)){
		release_mutex(&proc->sig_lock);
		return 0;
	}

	proc->pending_sig |= sigmask(signum);

	//if blocked don't handle
	if(proc->sig_mask & sigmask(signum)){
		release_mutex(&proc->sig_lock);
		return 0;
	}
	
	//if the task is blocked interrupt it
	//FIXME : maybee we should acquire the state lock here
	if(proc->flags & PROC_FLAG_BLOCKED){
		proc->flags |= PROC_FLAG_INTR;
		unblock_proc(proc);
	}

	release_mutex(&proc->sig_lock);
	return 0;
}

void handle_signal(fault_frame *context){
	acquire_mutex(&get_current_proc()->sig_lock);
	sigset_t to_handle = get_current_proc()->pending_sig & ~get_current_proc()->sig_mask;

	//nothing to handle ? just return
	if(!to_handle){
		release_mutex(&get_current_proc()->sig_lock);
		return;
	}

	for (int signum = 1; signum < NSIG; signum++){
		if(to_handle & sigmask(signum)){
			kdebugf("signal %d recived\n",signum);
			get_current_proc()->pending_sig &= ~sigmask(signum);
			if(get_current_proc()->sig_handling[signum].sa_handler == SIG_IGN){
				continue;
			} else if(get_current_proc()->sig_handling[signum].sa_handler == SIG_DFL){
				handle_default(signum);
				continue;
			} else {
				release_mutex(&get_current_proc()->sig_lock);
				//this is the tricky part
				uintptr_t sp = SP_REG(*context);
				kdebugf("sp : %p\n",sp);
				//align the stack
				sp &= ~0xfUL;
				//we need make the ucontext on the userspace stack
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
			}
		}
	}
	release_mutex(&get_current_proc()->sig_lock);
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