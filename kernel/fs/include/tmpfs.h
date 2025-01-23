#ifndef TMPFS_H
#define TMPFS_H
#include "vfs.h"

void init_tmpfs();
vfs_node *new_tmpfs();

struct tmpfs_inode_struct;

typedef struct {
	uint64_t (* read)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	uint64_t (* write)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	int (* close)(struct vfs_node_struct *);
	int (* create)(struct vfs_node_struct*,char *name,mode_t perm);
	int (* mkdir)(struct vfs_node_struct*,char *name,mode_t perm);
	int (* unlink)(struct vfs_node_struct*,char *);
	int (* truncate)(struct vfs_node_struct*,size_t);
	int (* ioctl)(struct vfs_node_struct*,uint64_t,void*);
}device_op;

typedef struct {
	char name[256];
	struct tmpfs_inode_struct *child;
	struct tmpfs_inode_struct *brother;
	struct tmpfs_inode_struct *parent;
	uint64_t children_count;
	uint64_t flags;
	size_t buffer_size;
	char *buffer;
	device_op *dev_op; //only use for dev
}tmpfs_inode;

vfs_node *tmpfs_finddir(vfs_node *node,const char *name);
uint64_t tmpfs_read(vfs_node *node,const void *buffer,uint64_t offset,size_t count);
uint64_t tmpfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count);
void tmpfs_close(vfs_node *node);
int tmpfs_create(vfs_node *node,const char *name,int perm);
int tmpfs_mkdir(vfs_node *node,const char *name,int perm);
int tmpfs_unlink(vfs_node *node,const char *name);
struct dirent *tmpfs_readdir(vfs_node *node,uint64_t index);
int tmpfs_truncate(vfs_node *node,size_t size);
int tmpfs_ioctl(vfs_node *node,uint64_t request,void *arg);

#define TMPFS_FLAGS_FILE 0x01
#define TMPFS_FLAGS_DIR  0x02
#define TMPFS_FLAG_DEV 0x04

#define IOCTL_TMPFS_CREATE_DEV 0x01

#endif