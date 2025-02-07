#ifndef SYS_H
#define SYS_H

#include <stdint.h>
#include "scheduler.h"

void sys_exit(uint64_t error_code);
pid_t sys_getpid();
void init_syscall(void);

#endif