#ifndef KERNEL_SIGNAL_H
#define KERNEL_SIGNAL_H

#include <kernel/scheduler.h>
#include <kernel/arch.h>

int send_sig(process *proc,int signum);
int send_sig_pgrp(pid_t pgrp,int signum);
void handle_signal(fault_frame *context);
void restore_signal_handler(fault_frame *context);

#define MAGIC_SIGRETURN 0x4848

#endif