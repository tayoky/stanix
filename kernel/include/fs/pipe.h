#ifndef PIPE_H
#define PIPE_H

#include <kernel/vfs.h>

int create_pipe(vfs_fd_t **read,vfs_fd_t **write);

#endif
