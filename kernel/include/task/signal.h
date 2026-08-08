#ifndef KERNEL_SIGNAL_H
#define KERNEL_SIGNAL_H

#include <kernel/arch.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>

int send_sig_task(task_t *thread, int signum);
int send_sig(process_t *proc, int signum);
int send_sig_group(process_group_t *group, int signum);
void handle_signal(registers_t *context);
void restore_signal_handler(registers_t *context);

#define MAGIC_SIGRETURN 0x4848

#endif
