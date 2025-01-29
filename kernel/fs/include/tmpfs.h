#ifndef TMPFS_H
#define TMPFS_H
#include "vfs.h"

void init_tmpfs();
vfs_node *new_tmpfs();

struct tmpfs_inode_struct;

typedef struct tmpfs_inode_struct{
	char name[256];
	struct tmpfs_inode_struct *child;
	struct tmpfs_inode_struct *brother;
	struct tmpfs_inode_struct *parent;
	uint64_t children_count;
	uint64_t flags;
	size_t buffer_size;
	char *buffer;
	device_op *dev_op; //only use for dev
	void *dev_inode;   //only use for dev
	mode_t perm;
	uid_t owner;
	gid_t group_owner;
}tmpfs_inode;

vfs_node *tmpfs_finddir(vfs_node *node,const char *name);
int64_t tmpfs_read(vfs_node *node,void *buffer,uint64_t offset,size_t count);
int64_t tmpfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count);
void tmpfs_close(vfs_node *node);
int tmpfs_create(vfs_node *node,const char *name,int perm,uint64_t flags);
int tmpfs_unlink(vfs_node *node,const char *name);
struct dirent *tmpfs_readdir(vfs_node *node,uint64_t index);
int tmpfs_truncate(vfs_node *node,size_t size);
int tmpfs_create_dev(vfs_node *node,const char *name,device_op *op,void *dev_inode);
int tmpfs_chmod(vfs_node *node,mode_t perm);
int tmpfs_chown(vfs_node *node,uid_t owner,gid_t group_owner);

#define TMPFS_FLAGS_FILE 0x01
#define TMPFS_FLAGS_DIR  0x02
#define TMPFS_FLAG_DEV 0x04

#define IOCTL_TMPFS_CREATE_DEV 0x01
#define IOCTL_TMPFS_SET_DEV_INODE 0x02

#endif