#ifndef PIPE_H
#define PIPE_H

#include <kernel/vfs.h>

int create_pipe(vfs_fd_t **read,vfs_fd_t **write);
ssize_t pipe_read(vfs_fd_t *fd,void *buffer,uint64_t offset,size_t count);
ssize_t pipe_write(vfs_fd_t *fd,void *buffer,uint64_t offset,size_t count);
int pipe_wait_check(vfs_fd_t *fd,short type);
void pipe_close(vfs_fd_t *fd);

#endif
