#ifndef KERNEL_SYSFS_H
#define KERNEL_SYSFS_H

#include <kernel/list.h>
#include <kernel/vfs.h>

typedef struct sysfs_inode {
	vfs_node_t vnode;
	int type;
	void *ptr;
} sysfs_inode_t;

void init_sysfs(void);

#endif
