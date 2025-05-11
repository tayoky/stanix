#ifndef PIPE_H
#define PIPE_H

#include <kernel/vfs.h>

int create_pipe(vfs_node **read,vfs_node **write);
ssize_t pipe_read(vfs_node *node,void *buffer,uint64_t offset,size_t count);
ssize_t pipe_write(vfs_node *node,void *buffer,uint64_t offset,size_t count);
int pipe_wait_check(vfs_node *node,short type);
vfs_node *pipe_dup(vfs_node *node);
void pipe_close(vfs_node *node);

#endif
