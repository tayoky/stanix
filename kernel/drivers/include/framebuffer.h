#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H
#include "vfs.h"

void init_frambuffer(void);


uint64_t framebuffer_write(vfs_node *node,void *buffer,uint64_t offset,size_t count);
int framebuffer_ioctl(vfs_node *node,uint64_t request,void *arg);

#define IOCTL_FRAMEBUFFER_WIDTH  0x01
#define IOCTL_FRAMEBUFFER_HEIGHT 0x02
#endif