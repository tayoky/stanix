#ifndef _KERNEL_SYSFS_H
#define _KERNEL_SYSFS_H

#include <kernel/vfs.h>
#include <kernel/list.h>

typedef struct sysfs_inode {
    const char *name;
    list *child;
    int type;
    vfs_node *linked_node;
} sysfs_inode;

void init_sysfs(void);
void sysfs_register(const char *name,vfs_node *node);

#endif