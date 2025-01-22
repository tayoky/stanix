#ifndef DEV_H
#define DEV_H
#include <stdint.h>
#include <stddef.h>
#include "vfs.h"

typedef struct {
	uint64_t (* read)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	uint64_t (* write)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	int (* close)(struct vfs_node_struct *);
	int (* create)(struct vfs_node_struct*,char *name,mode_t perm);
	int (* mkdir)(struct vfs_node_struct*,char *name,mode_t perm);
	int (* unlink)(struct vfs_node_struct*,char *);
	int (* truncate)(struct vfs_node_struct*,size_t);
}device_op;

void init_devices(void);

int create_dev(const char *path,device_op *device);

#endif