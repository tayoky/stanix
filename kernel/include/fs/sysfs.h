#ifndef _KERNEL_SYSFS_H
#define _KERNEL_SYSFS_H

#include <kernel/vfs.h>
#include <kernel/list.h>

typedef struct sysfs_inode {
	vfs_node_t node;
    int type;
	void *ptr;
} sysfs_inode_t;

void init_sysfs(void);
void sysfs_register(const char *name,vfs_node_t *node);

#endif