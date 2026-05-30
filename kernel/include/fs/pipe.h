#ifndef KERNEL_PIPE_H
#define KERNEL_PIPE_H

#include <kernel/vfs.h>

int pipe_create(vfs_fd_t **read,vfs_fd_t **write);

#endif
