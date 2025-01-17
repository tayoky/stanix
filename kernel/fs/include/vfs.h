#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_MAX_NODE_NAME_LEN 256
#define VFS_MAX_MOUNT_POINT_NAME_LEN 128

struct vfs_node_struct;

typedef struct vfs_node_struct {
	void *private_inode;
	uint64_t (* read)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	uint64_t (* write)(struct vfs_node_struct *,void *buf,uint64_t off,size_t count);
	uint64_t (* close)(struct vfs_node_struct *);
	struct vfs_node_struct (* finddir)(struct vfs_node_struct *,char *name);
}vfs_node;

struct vfs_mount_point_struct;

typedef struct vfs_mount_point_struct{
	char name[VFS_MAX_MOUNT_POINT_NAME_LEN];
	struct vfs_mount_point_struct *prev;
	struct vfs_mount_point_struct *next;
	vfs_node *root;
}vfs_mount_point;


struct kernel_table_struct;
void init_vfs(void);

int vfs_mount(const char *name,vfs_node *mounting_root);


vfs_node finddir(vfs_node *node,const char *name);
uint64_t vfs_read(vfs_node *node,const void *buffer,uint64_t offset,size_t count);
uint64_t vfs_write(vfs_node *node,void *buffer,uint64_t offset,size_t count);
void vfs_close(vfs_node *node);
#endif