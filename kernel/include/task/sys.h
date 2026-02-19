#ifndef SYS_H
#define SYS_H

#include <stdint.h>
#include <kernel/arch.h>
#include <kernel/scheduler.h>

void init_syscall(void);

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

void syscall_handler(fault_frame_t *context, void *arg);

#endif