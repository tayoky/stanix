#ifndef KERNEL_CORE_H
#define KERNEL_CORE_H

#include <kernel/signal.h>
#include <kernel/vfs.h>

int core_write(vfs_fd_t *fd, siginfo_t *siginfo);
int core_dump(siginfo_t *siginfo);

#endif
