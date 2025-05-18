#ifndef KERNEL_SIGNAL_H
#define KERNEL_SIGNAL_H

#include <kernel/scheduler.h>

int send_sig(process *proc,int signum);

#endif